/*
    SPDX-FileCopyrightText: 2008 David Nolden <david.nolden.kdevelop@art-master.de>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "codemodel.h"

#include "appendedlist.h"
#include <debug.h>
#include <serialization/itemrepository.h>
#include "identifier.h"
#include <serialization/indexedstring.h>
#include <serialization/referencecounting.h>
#include <util/embeddedfreetree.h>

#define ifDebug(x)

namespace KDevelop {
class CodeModelItemHandler
{
public:
    static int leftChild(const CodeModelItem& m_data)
    {
        return ( int )m_data.referenceCount;
    }
    static void setLeftChild(CodeModelItem& m_data, int child)
    {
        m_data.referenceCount = ( uint )child;
    }
    static int rightChild(const CodeModelItem& m_data)
    {
        return ( int )m_data.uKind;
    }
    static void setRightChild(CodeModelItem& m_data, int child)
    {
        m_data.uKind = ( uint )child;
    }
    //Copies this item into the given one
    static void copyTo(const CodeModelItem& m_data, CodeModelItem& data)
    {
        data = m_data;
    }

    static void createFreeItem(CodeModelItem& data)
    {
        data = CodeModelItem();
        data.referenceCount = (uint) - 1;
        data.uKind = (uint) - 1;
    }

    static bool isFree(const CodeModelItem& m_data)
    {
        return !m_data.id.isValid();
    }

    static const CodeModelItem& data(const CodeModelItem& m_data)
    {
        return m_data;
    }

    static bool equals(const CodeModelItem& m_data, const CodeModelItem& rhs)
    {
        return m_data.id == rhs.id;
    }
};

DEFINE_LIST_MEMBER_HASH(CodeModelRepositoryItem, items, CodeModelItem)

class CodeModelRepositoryItem
{
public:
    CodeModelRepositoryItem()
    {
        initializeAppendedLists();
    }
    CodeModelRepositoryItem(const CodeModelRepositoryItem& rhs, bool dynamic = true) : file(rhs.file)
        , centralFreeItem(rhs.centralFreeItem)
    {
        initializeAppendedLists(dynamic);
        copyListsFrom(rhs);
    }

    ~CodeModelRepositoryItem()
    {
        freeAppendedLists();
    }

    CodeModelRepositoryItem& operator=(const CodeModelRepositoryItem& rhs) = delete;

    unsigned int hash() const
    {
        //We only compare the declaration. This allows us implementing a map, although the item-repository
        //originally represents a set.
        return file.index();
    }

    uint itemSize() const
    {
        return dynamicSize();
    }

    uint classSize() const
    {
        return sizeof(CodeModelRepositoryItem);
    }

    IndexedString file;
    int centralFreeItem = -1;

    START_APPENDED_LISTS(CodeModelRepositoryItem);
    APPENDED_LIST_FIRST(CodeModelRepositoryItem, CodeModelItem, items);
    END_APPENDED_LISTS(CodeModelRepositoryItem, items);
};

class CodeModelRequestItem
{
public:

    CodeModelRequestItem(const CodeModelRepositoryItem& item) : m_item(item)
    {
    }
    enum {
        AverageSize = 30 //This should be the approximate average size of an Item
    };

    unsigned int hash() const
    {
        return m_item.hash();
    }

    uint itemSize() const
    {
        return m_item.itemSize();
    }

    void createItem(CodeModelRepositoryItem* item) const
    {
        Q_ASSERT(shouldDoDUChainReferenceCounting(item));
        Q_ASSERT(shouldDoDUChainReferenceCounting(reinterpret_cast<char*>(item) + (itemSize() - 1)));
        new (item) CodeModelRepositoryItem(m_item, false);
    }

    static void destroy(CodeModelRepositoryItem* item, KDevelop::AbstractItemRepository&)
    {
        Q_ASSERT(shouldDoDUChainReferenceCounting(item));
//     Q_ASSERT(shouldDoDUChainReferenceCounting(((char*)item) + (itemSize()-1)));
        item->~CodeModelRepositoryItem();
    }

    static bool persistent(const CodeModelRepositoryItem* item)
    {
        Q_UNUSED(item);
        return true;
    }

    bool equals(const CodeModelRepositoryItem* item) const
    {
        return m_item.file == item->file;
    }

    const CodeModelRepositoryItem& m_item;
};

using CodeModelRepo = ItemRepository<CodeModelRepositoryItem, CodeModelRequestItem>;
template <>
class ItemRepositoryFor<CodeModel>
{
    friend struct LockedItemRepository;
    static CodeModelRepo& repo()
    {
        static QMutex mutex;
        // Maps declaration-ids to items
        static CodeModelRepo repo(QStringLiteral("Code Model"), &mutex);
        return repo;
    }
};

class CodeModelPrivate
{
};

CodeModel::CodeModel()
    : d_ptr(nullptr)
{
}

CodeModel::~CodeModel() = default;

void CodeModel::addItem(const IndexedString& file, const IndexedQualifiedIdentifier& id, CodeModelItem::Kind kind)
{
    ifDebug(qCDebug(LANGUAGE) << "addItem" << file.str() << id.identifier().toString() << id.index; )

    if (!id.isValid())
        return;
    CodeModelRepositoryItem item;
    item.file = file;
    CodeModelRequestItem request(item);

    CodeModelItem newItem;
    newItem.id = id;
    newItem.kind = kind;
    newItem.referenceCount = 1;

    LockedItemRepository::op<CodeModel>([&](CodeModelRepo& repo) {
        uint index = repo.findIndex(item);

        if (index) {
            const CodeModelRepositoryItem* oldItem = repo.itemFromIndex(index);
            EmbeddedTreeAlgorithms<CodeModelItem, CodeModelItemHandler> alg(oldItem->items(), oldItem->itemsSize(),
                                                                            oldItem->centralFreeItem);

            int listIndex = alg.indexOf(newItem);

            DynamicItem<CodeModelRepositoryItem, true> editableItem = repo.dynamicItemFromIndex(index);
            auto* items = const_cast<CodeModelItem*>(editableItem->items());

            if (listIndex != -1) {
                // Only update the reference-count
                ++items[listIndex].referenceCount;
                items[listIndex].kind = kind;
                return;
            } else {
                // Add the item to the list
                EmbeddedTreeAddItem<CodeModelItem, CodeModelItemHandler> add(items, editableItem->itemsSize(),
                                                                             editableItem->centralFreeItem, newItem);

                if (add.newItemCount() != editableItem->itemsSize()) {
                    // The data needs to be transferred into a bigger list. That list is within "item".

                    item.itemsList().resize(add.newItemCount());
                    add.transferData(item.itemsList().data(), item.itemsList().size(), &item.centralFreeItem);

                    repo.deleteItem(index);
                } else {
                    // We're fine: The item fits into the existing list.
                    return;
                }
            }
        } else {
            // We're creating a new index
            item.itemsList().append(newItem);
        }

        Q_ASSERT(!repo.findIndex(request));

        // This inserts the changed item
        const uint newIndex = repo.index(request);
        Q_UNUSED(newIndex);
        ifDebug(qCDebug(LANGUAGE) << "new index" << newIndex;)

        Q_ASSERT(repo.findIndex(request));
    });
}

void CodeModel::updateItem(const IndexedString& file, const IndexedQualifiedIdentifier& id, CodeModelItem::Kind kind)
{
    ifDebug(qCDebug(LANGUAGE) << file.str() << id.identifier().toString() << kind; )

    if (!id.isValid())
        return;

    CodeModelRepositoryItem item;
    item.file = file;
    CodeModelRequestItem request(item);

    CodeModelItem newItem;
    newItem.id = id;
    newItem.kind = kind;
    newItem.referenceCount = 1;

    LockedItemRepository::op<CodeModel>([&](CodeModelRepo& repo) {
        uint index = repo.findIndex(item);

        if (index) {
            // Check whether the item is already in the mapped list, else copy the list into the new created item
            DynamicItem<CodeModelRepositoryItem, true> oldItem = repo.dynamicItemFromIndex(index);

            EmbeddedTreeAlgorithms<CodeModelItem, CodeModelItemHandler> alg(oldItem->items(), oldItem->itemsSize(),
                                                                            oldItem->centralFreeItem);
            int listIndex = alg.indexOf(newItem);
            Q_ASSERT(listIndex != -1);

            auto* items = const_cast<CodeModelItem*>(oldItem->items());

            Q_ASSERT(items[listIndex].id == id);
            items[listIndex].kind = kind;

            return;
        }

        Q_ASSERT(0); // The updated item as not in the symbol table!
    });
}

void CodeModel::removeItem(const IndexedString& file, const IndexedQualifiedIdentifier& id)
{
    if (!id.isValid())
        return;

    ifDebug(qCDebug(LANGUAGE) << "removeItem" << file.str() << id.identifier().toString(); )
    CodeModelRepositoryItem item;
    item.file = file;
    CodeModelRequestItem request(item);

    LockedItemRepository::op<CodeModel>([&](CodeModelRepo& repo) {
        uint index = repo.findIndex(item);

        if (index) {
            CodeModelItem searchItem;
            searchItem.id = id;

            DynamicItem<CodeModelRepositoryItem, true> oldItem = repo.dynamicItemFromIndex(index);

            EmbeddedTreeAlgorithms<CodeModelItem, CodeModelItemHandler> alg(oldItem->items(), oldItem->itemsSize(),
                                                                            oldItem->centralFreeItem);

            int listIndex = alg.indexOf(searchItem);
            if (listIndex == -1)
                return;

            auto* items = const_cast<CodeModelItem*>(oldItem->items());

            --items[listIndex].referenceCount;

            if (oldItem->items()[listIndex].referenceCount)
                return; // Nothing to remove, there's still a reference-count left

            // We have reduced the reference-count to zero, so remove the item from the list

            EmbeddedTreeRemoveItem<CodeModelItem, CodeModelItemHandler> remove(items, oldItem->itemsSize(),
                                                                               oldItem->centralFreeItem, searchItem);

            uint newItemCount = remove.newItemCount();
            if (newItemCount != oldItem->itemsSize()) {
                if (newItemCount == 0) {
                    // Has become empty, delete the item
                    repo.deleteItem(index);

                    return;
                } else {
                    // Make smaller
                    item.itemsList().resize(newItemCount);
                    remove.transferData(item.itemsList().data(), item.itemsSize(), &item.centralFreeItem);

                    // Delete the old list
                    repo.deleteItem(index);
                    // Add the new list
                    repo.index(request);
                    return;
                }
            }
        }
    });
}

void CodeModel::items(const IndexedString& file, uint& count, const CodeModelItem*& items) const
{
    ifDebug(qCDebug(LANGUAGE) << "items" << file.str(); )

    CodeModelRepositoryItem item;
    item.file = file;
    CodeModelRequestItem request(item);

    LockedItemRepository::op<CodeModel>([&](CodeModelRepo& repo) {
        uint index = repo.findIndex(item);

        if (index) {
            const CodeModelRepositoryItem* repositoryItem = repo.itemFromIndex(index);
            ifDebug(qCDebug(LANGUAGE) << "found index" << index << repositoryItem->itemsSize();)
            count = repositoryItem->itemsSize();
            items = repositoryItem->items();
        } else {
            ifDebug(qCDebug(LANGUAGE) << "found no index";)
            count = 0;
            items = nullptr;
        }
    });
}

CodeModel& CodeModel::self()
{
    static CodeModel ret;
    return ret;
}
}
