/*
    SPDX-FileCopyrightText: 2008 David Nolden <david.nolden.kdevelop@art-master.de>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "persistentsymboltable.h"

#include <QHash>

#include "declaration.h"
#include "declarationid.h"
#include "appendedlist.h"
#include "serialization/itemrepository.h"
#include "identifier.h"
#include "ducontext.h"
#include "topducontext.h"
#include "duchain.h"
#include "duchainlock.h"
#include <util/embeddedfreetree.h>

//For now, just _always_ use the cache
const uint MinimumCountForCache = 1;

namespace {
QDebug fromTextStream(const QTextStream& out)
{
    if (out.device())
        return {out.device()};
    return {out.string()};
}
}

namespace KDevelop {

using TextStreamFunction = QTextStream& (*)(QTextStream&);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
constexpr TextStreamFunction endl = Qt::endl;
#else
constexpr TextStreamFunction endl = ::endl;
#endif

Utils::BasicSetRepository* RecursiveImportCacheRepository::repository()
{
    static QRecursiveMutex mutex;
    static Utils::BasicSetRepository recursiveImportCacheRepositoryObject(QStringLiteral("Recursive Imports Cache"),
                                                                          &mutex, nullptr, false);
    return &recursiveImportCacheRepositoryObject;
}

DEFINE_LIST_MEMBER_HASH(PersistentSymbolTableItem, declarations, IndexedDeclaration)

class PersistentSymbolTableItem
{
public:
    PersistentSymbolTableItem() : centralFreeItem(-1)
    {
        initializeAppendedLists();
    }
    PersistentSymbolTableItem(const PersistentSymbolTableItem& rhs, bool dynamic = true) : id(rhs.id)
        , centralFreeItem(rhs.centralFreeItem)
    {
        initializeAppendedLists(dynamic);
        copyListsFrom(rhs);
    }

    ~PersistentSymbolTableItem()
    {
        freeAppendedLists();
    }

    PersistentSymbolTableItem& operator=(const PersistentSymbolTableItem& rhs) = delete;

    inline unsigned int hash() const
    {
        //We only compare the declaration. This allows us implementing a map, although the item-repository
        //originally represents a set.
        return id.index();
    }

    unsigned int itemSize() const
    {
        return dynamicSize();
    }

    uint classSize() const
    {
        return sizeof(PersistentSymbolTableItem);
    }

    IndexedQualifiedIdentifier id;
    int centralFreeItem;

    START_APPENDED_LISTS(PersistentSymbolTableItem);
    APPENDED_LIST_FIRST(PersistentSymbolTableItem, IndexedDeclaration, declarations);
    END_APPENDED_LISTS(PersistentSymbolTableItem, declarations);
};

class PersistentSymbolTableRequestItem
{
public:

    PersistentSymbolTableRequestItem(const PersistentSymbolTableItem& item) : m_item(item)
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

    void createItem(PersistentSymbolTableItem* item) const
    {
        new (item) PersistentSymbolTableItem(m_item, false);
    }

    static void destroy(PersistentSymbolTableItem* item, KDevelop::AbstractItemRepository&)
    {
        item->~PersistentSymbolTableItem();
    }

    static bool persistent(const PersistentSymbolTableItem*)
    {
        return true; //Nothing to do
    }

    bool equals(const PersistentSymbolTableItem* item) const
    {
        return m_item.id == item->id;
    }

    const PersistentSymbolTableItem& m_item;
};

template <class ValueType>
struct CacheEntry
{
    using Data = KDevVarLengthArray<ValueType>;
    using DataHash = QHash<TopDUContext::IndexedRecursiveImports, Data>;

    DataHash m_hash;
};

using PersistentSymbolTableRepo = ItemRepository<PersistentSymbolTableItem, PersistentSymbolTableRequestItem>;

template <>
class ItemRepositoryFor<PersistentSymbolTableItem>
{
    friend struct LockedItemRepository;
    static PersistentSymbolTableRepo& repo()
    {
        static QMutex mutex;
        // Maps declaration-ids to declarations
        //  mutable as things like findIndex are not const
        static PersistentSymbolTableRepo repo { QStringLiteral("Persistent Declaration Table"), &mutex };
        return repo;
    }

public:
    static void init() { repo(); }
};

class PersistentSymbolTablePrivate
{
public:
    mutable QHash<IndexedQualifiedIdentifier, CacheEntry<IndexedDeclaration>> m_declarationsCache;

    //We cache the imports so the currently used nodes are very close in memory, which leads to much better CPU cache utilization
    mutable QHash<TopDUContext::IndexedRecursiveImports, PersistentSymbolTable::CachedIndexedRecursiveImports> m_importsCache;

    void addDeclaration(const IndexedQualifiedIdentifier& id, const IndexedDeclaration& declaration);
    void removeDeclaration(const IndexedQualifiedIdentifier& id, const IndexedDeclaration& declaration);
    void declarations(const IndexedQualifiedIdentifier& id, uint& count, const IndexedDeclaration*& declarations) const;
    PersistentSymbolTable::Declarations declarations(const IndexedQualifiedIdentifier& id) const;
    PersistentSymbolTable::FilteredDeclarationIterator
    filteredDeclarations(const IndexedQualifiedIdentifier& id,
                         const TopDUContext::IndexedRecursiveImports& visibility) const;
};

void PersistentSymbolTable::clearCache()
{
    Q_D(PersistentSymbolTable);

    ENSURE_CHAIN_WRITE_LOCKED
    d->m_importsCache.clear();
    d->m_declarationsCache.clear();
}

PersistentSymbolTable::PersistentSymbolTable()
    : d_ptr(new PersistentSymbolTablePrivate())
{
    ItemRepositoryFor<PersistentSymbolTableItem>::init();
}

PersistentSymbolTable::~PersistentSymbolTable()
{
    //Workaround for a strange destruction-order related crash duing shutdown
    //We just let the data leak. This doesn't hurt, as there is no meaningful destructors.
    // TODO: analyze and fix it
//   delete d;
}

void PersistentSymbolTable::addDeclaration(const IndexedQualifiedIdentifier& id, const IndexedDeclaration& declaration)
{
    Q_D(PersistentSymbolTable);

    d->addDeclaration(id, declaration);
}

void PersistentSymbolTablePrivate::addDeclaration(const IndexedQualifiedIdentifier& id,
                                                  const IndexedDeclaration& declaration)
{
    ENSURE_CHAIN_WRITE_LOCKED

    m_declarationsCache.remove(id);

    LockedItemRepository::write<PersistentSymbolTableItem>([&id, &declaration](PersistentSymbolTableRepo& repo) {
        PersistentSymbolTableItem item;
        item.id = id;

        uint index = repo.findIndex(item);

        if (index) {
            // Check whether the item is already in the mapped list, else copy the list into the new created item
            const PersistentSymbolTableItem* oldItem = repo.itemFromIndex(index);

            EmbeddedTreeAlgorithms<IndexedDeclaration, IndexedDeclarationHandler> alg(
                oldItem->declarations(), oldItem->declarationsSize(), oldItem->centralFreeItem);

            if (alg.indexOf(declaration) != -1)
                return;

            DynamicItem<PersistentSymbolTableItem, true> editableItem = repo.dynamicItemFromIndex(index);

            EmbeddedTreeAddItem<IndexedDeclaration, IndexedDeclarationHandler> add(
                const_cast<IndexedDeclaration*>(editableItem->declarations()), editableItem->declarationsSize(),
                editableItem->centralFreeItem, declaration);

            uint newSize = add.newItemCount();
            if (newSize != editableItem->declarationsSize()) {
                // We need to resize. Update and fill the new item, and delete the old item.
                item.declarationsList().resize(newSize);
                add.transferData(item.declarationsList().data(), newSize, &item.centralFreeItem);

                repo.deleteItem(index);
                Q_ASSERT(!repo.findIndex(item));
            } else {
                // We're fine, the item could be added to the existing list
                return;
            }
        } else {
            item.declarationsList().append(declaration);
        }

        // This inserts the changed item
        repo.index(item);
    });
}

void PersistentSymbolTable::removeDeclaration(const IndexedQualifiedIdentifier& id,
                                              const IndexedDeclaration& declaration)
{
    Q_D(PersistentSymbolTable);

    d->removeDeclaration(id, declaration);
}

void PersistentSymbolTablePrivate::removeDeclaration(const IndexedQualifiedIdentifier& id,
                                                     const IndexedDeclaration& declaration)
{
    ENSURE_CHAIN_WRITE_LOCKED

    m_declarationsCache.remove(id);
    Q_ASSERT(!m_declarationsCache.contains(id));

    LockedItemRepository::write<PersistentSymbolTableItem>([&id, &declaration](PersistentSymbolTableRepo& repo) {
        PersistentSymbolTableItem item;
        item.id = id;

        uint index = repo.findIndex(item);

        if (index) {
            // Check whether the item is already in the mapped list, else copy the list into the new created item
            const PersistentSymbolTableItem* oldItem = repo.itemFromIndex(index);

            EmbeddedTreeAlgorithms<IndexedDeclaration, IndexedDeclarationHandler> alg(
                oldItem->declarations(), oldItem->declarationsSize(), oldItem->centralFreeItem);

            if (alg.indexOf(declaration) == -1)
                return;

            DynamicItem<PersistentSymbolTableItem, true> editableItem = repo.dynamicItemFromIndex(index);

            EmbeddedTreeRemoveItem<IndexedDeclaration, IndexedDeclarationHandler> remove(
                const_cast<IndexedDeclaration*>(editableItem->declarations()), editableItem->declarationsSize(),
                editableItem->centralFreeItem, declaration);

            uint newSize = remove.newItemCount();
            if (newSize != editableItem->declarationsSize()) {
                // We need to resize. Update and fill the new item, and delete the old item.
                item.declarationsList().resize(newSize);
                remove.transferData(item.declarationsList().data(), newSize, &item.centralFreeItem);

                repo.deleteItem(index);
                Q_ASSERT(!repo.findIndex(item));
            } else {
                // We're fine, the item could be added to the existing list
                return;
            }
        }

        // This inserts the changed item
        if (item.declarationsSize())
            repo.index(item);
    });
}

struct DeclarationCacheVisitor
{
    explicit DeclarationCacheVisitor(KDevVarLengthArray<IndexedDeclaration>& _cache) : cache(_cache)
    {
    }

    bool operator()(const IndexedDeclaration& decl) const
    {
        cache.append(decl);
        return true;
    }

    KDevVarLengthArray<IndexedDeclaration>& cache;
};

PersistentSymbolTable::FilteredDeclarationIterator PersistentSymbolTable::filteredDeclarations(
    const IndexedQualifiedIdentifier& id, const TopDUContext::IndexedRecursiveImports& visibility) const
{
    Q_D(const PersistentSymbolTable);

    return d->filteredDeclarations(id, visibility);
}

PersistentSymbolTable::FilteredDeclarationIterator
PersistentSymbolTablePrivate::filteredDeclarations(const IndexedQualifiedIdentifier& id,
                                                   const TopDUContext::IndexedRecursiveImports& visibility) const
{
    ENSURE_CHAIN_READ_LOCKED

    PersistentSymbolTable::Declarations decls = declarations(id).iterator();

    PersistentSymbolTable::CachedIndexedRecursiveImports cachedImports;

    auto it = m_importsCache.constFind(visibility);
    if (it != m_importsCache.constEnd()) {
        cachedImports = *it;
    } else {
        cachedImports = PersistentSymbolTable::CachedIndexedRecursiveImports(visibility.set().stdSet());
        m_importsCache.insert(visibility, cachedImports);
    }

    if (decls.dataSize() > MinimumCountForCache) {
        //Do visibility caching
        CacheEntry<IndexedDeclaration>& cached(m_declarationsCache[id]);
        CacheEntry<IndexedDeclaration>::DataHash::const_iterator cacheIt = cached.m_hash.constFind(visibility);
        if (cacheIt != cached.m_hash.constEnd())
            return PersistentSymbolTable::FilteredDeclarationIterator(
                PersistentSymbolTable::Declarations::Iterator(cacheIt->constData(), cacheIt->size(), -1),
                cachedImports);

        CacheEntry<IndexedDeclaration>::DataHash::iterator insertIt = cached.m_hash.insert(visibility,
                                                                                           KDevVarLengthArray<IndexedDeclaration>());

        KDevVarLengthArray<IndexedDeclaration>& cache(*insertIt);

        {
            using FilteredDeclarationCacheVisitor
                = ConvenientEmbeddedSetTreeFilterVisitor<IndexedDeclaration, IndexedDeclarationHandler,
                                                         IndexedTopDUContext,
                                                         PersistentSymbolTable::CachedIndexedRecursiveImports,
                                                         DeclarationTopContextExtractor, DeclarationCacheVisitor>;

            //The visitor visits all the declarations from within its constructor
            DeclarationCacheVisitor v(cache);
            FilteredDeclarationCacheVisitor visitor(v, decls.iterator(), cachedImports);
        }

        return PersistentSymbolTable::FilteredDeclarationIterator(
            PersistentSymbolTable::Declarations::Iterator(cache.constData(), cache.size(), -1), cachedImports, true);
    } else {
        return PersistentSymbolTable::FilteredDeclarationIterator(decls.iterator(), cachedImports);
    }
}

PersistentSymbolTable::Declarations PersistentSymbolTable::declarations(const IndexedQualifiedIdentifier& id) const
{
    Q_D(const PersistentSymbolTable);

    return d->declarations(id);
}

PersistentSymbolTable::Declarations
PersistentSymbolTablePrivate::declarations(const IndexedQualifiedIdentifier& id) const
{
    ENSURE_CHAIN_READ_LOCKED

    PersistentSymbolTableItem item;
    item.id = id;

    return LockedItemRepository::read<PersistentSymbolTableItem>([&item](const PersistentSymbolTableRepo& repo) {
        uint index = repo.findIndex(item);

        if (index) {
            const PersistentSymbolTableItem* repositoryItem = repo.itemFromIndex(index);
            return PersistentSymbolTable::Declarations(
                repositoryItem->declarations(), repositoryItem->declarationsSize(), repositoryItem->centralFreeItem);
        } else {
            return PersistentSymbolTable::Declarations();
        }
    });
}

void PersistentSymbolTable::declarations(const IndexedQualifiedIdentifier& id, uint& countTarget,
                                         const IndexedDeclaration*& declarationsTarget) const
{
    Q_D(const PersistentSymbolTable);

    return d->declarations(id, countTarget, declarationsTarget);
}

void PersistentSymbolTablePrivate::declarations(const IndexedQualifiedIdentifier& id, uint& countTarget,
                                                const IndexedDeclaration*& declarationsTarget) const
{
    ENSURE_CHAIN_READ_LOCKED

    PersistentSymbolTableItem item;
    item.id = id;

    LockedItemRepository::read<PersistentSymbolTableItem>(
        [&item, &countTarget, &declarationsTarget](const PersistentSymbolTableRepo& repo) {
            uint index = repo.findIndex(item);

            if (index) {
                const PersistentSymbolTableItem* repositoryItem = repo.itemFromIndex(index);
                countTarget = repositoryItem->declarationsSize();
                declarationsTarget = repositoryItem->declarations();
            } else {
                countTarget = 0;
                declarationsTarget = nullptr;
            }
        });
}

struct DebugVisitor
{
    explicit DebugVisitor(const QTextStream& _out)
        : out(_out)
    {
    }

    bool operator()(const PersistentSymbolTableItem* item)
    {
        QDebug qout = fromTextStream(out);
        QualifiedIdentifier id(item->id.identifier());
        if (identifiers.contains(id)) {
            qout << "identifier" << id.toString() << "appears for" << identifiers[id] << "th time";
        }

        ++identifiers[id];

        for (uint a = 0; a < item->declarationsSize(); ++a) {
            IndexedDeclaration decl(item->declarations()[a]);
            if (!decl.isDummy()) {
                if (declarations.contains(decl)) {
                    qout << "declaration found for multiple identifiers. Previous identifier:" <<
                        declarations[decl].toString() << "current identifier:" << id.toString() << endl;
                } else {
                    declarations.insert(decl, id);
                }
            }
            if (decl.data() && decl.data()->qualifiedIdentifier() != item->id.identifier()) {
                qout << decl.data()->url().str() << "declaration" << decl.data()->qualifiedIdentifier() <<
                    "is registered as" << item->id.identifier() << endl;
            }

            const QString url = IndexedTopDUContext(decl.topContextIndex()).url().str();
            if (!decl.data() && !decl.isDummy()) {
                qout << "Item in symbol-table is invalid:" << id.toString() << "- localIndex:" << decl.localIndex() <<
                    "- url:" << url << endl;
            } else {
                qout << "Item in symbol-table:" << id.toString() << "- localIndex:" << decl.localIndex() << "- url:" <<
                    url;
                if (auto d = decl.data()) {
                    qout << "- range:" << d->range();
                } else {
                    qout << "- null declaration";
                }
                qout << endl;
            }
        }

        return true;
    }

    const QTextStream& out;
    QHash<QualifiedIdentifier, uint> identifiers;
    QHash<IndexedDeclaration, QualifiedIdentifier> declarations;
};

void PersistentSymbolTable::dump(const QTextStream& out)
{
    QDebug qout = fromTextStream(out);
    DebugVisitor v(out);

    LockedItemRepository::read<PersistentSymbolTableItem>([&](const PersistentSymbolTableRepo& repo) {
        repo.visitAllItems(v);

        qout << "Statistics:" << endl;
        qout << repo.statistics() << endl;
    });
}

PersistentSymbolTable& PersistentSymbolTable::self()
{
    static PersistentSymbolTable ret;
    return ret;
}
}
