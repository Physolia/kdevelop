/*
    SPDX-FileCopyrightText: 2009 Milian Wolff <mail@milianw.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "colorcache.h"

#include "configurablecolors.h"
#include "codehighlighting.h"

#include <KColorScheme>

#include "../../interfaces/icore.h"
#include "../../interfaces/ilanguagecontroller.h"
#include "../../interfaces/icompletionsettings.h"
#include "../../interfaces/idocument.h"
#include "../../interfaces/idocumentcontroller.h"
#include "../interfaces/ilanguagesupport.h"
#include "../duchain/duchain.h"
#include "../duchain/duchainlock.h"
#include <debug.h>
#include "widgetcolorizer.h"

#include <ktexteditor_version.h>
#include <KTextEditor/Document>
#include <KTextEditor/View>
#include <KTextEditor/ConfigInterface>
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Format>

#define ifDebug(x)

namespace KDevelop {
ColorCache* ColorCache::m_self = nullptr;

HighlightingEnumContainer::Types getHighlightingTypeFromName(const QString& name)
{
    if (name == QLatin1String("Class")) {
        return CodeHighlightingInstance::ClassType;
    } else if (name == QLatin1String("Local Member Variable")) {
        return CodeHighlightingInstance::LocalClassMemberType;
    } else if (name == QLatin1String("Local Member Function")) {
        return CodeHighlightingInstance::LocalMemberFunctionType;
    } else if (name == QLatin1String("Inherited Member Variable")) {
        return CodeHighlightingInstance::InheritedClassMemberType;
    } else if (name == QLatin1String("Inherited Member Function")) {
        return CodeHighlightingInstance::InheritedMemberFunctionType;
    } else if (name == QLatin1String("Function")) {
        return CodeHighlightingInstance::FunctionType;
    } else if (name == QLatin1String("Function Argument")) {
        return CodeHighlightingInstance::FunctionVariableType;
    } else if (name == QLatin1String("Type Alias")) {
        return CodeHighlightingInstance::TypeAliasType;
    } else if (name == QLatin1String("Forward Declaration")) {
        return CodeHighlightingInstance::ForwardDeclarationType;
    } else if (name == QLatin1String("Namespace")) {
        return CodeHighlightingInstance::NamespaceType;
    } else if (name == QLatin1String("Local Variable")) {
        return CodeHighlightingInstance::LocalVariableType;
    } else if (name == QLatin1String("Global Variable")) {
        return CodeHighlightingInstance::GlobalVariableType;
    } else if (name == QLatin1String("Member Variable")) {
        return CodeHighlightingInstance::MemberVariableType;
    } else if (name == QLatin1String("Namespace Variable")) {
        return CodeHighlightingInstance::NamespaceVariableType;
    } else if (name == QLatin1String("Enumeration")) {
        return CodeHighlightingInstance::EnumType;
    } else if (name == QLatin1String("Enumerator")) {
        return CodeHighlightingInstance::EnumeratorType;
    } else if (name == QLatin1String("Macro")) {
        return CodeHighlightingInstance::MacroType;
    } else if (name == QLatin1String("Macro Function")) {
        return CodeHighlightingInstance::MacroFunctionLikeType;
    } else if (name == QLatin1String("Highlight Uses")) {
        return CodeHighlightingInstance::HighlightUsesType;
    } else if (name == QLatin1String("Error Variable")) {
        return CodeHighlightingInstance::ErrorVariableType;
    }
    return CodeHighlightingInstance::UnknownType;
}

ColorCache::ColorCache(QObject* parent)
    : QObject(parent)
    , m_defaultColors(nullptr)
    , m_validColorCount(0)
    , m_colorOffset(0)
    , m_localColorRatio(0)
    , m_globalColorRatio(0)
    , m_boldDeclarations(true)
{
    Q_ASSERT(m_self == nullptr);

    updateColorsFromScheme(); // default / fallback
    updateColorsFromSettings();

    connect(ICore::self()->languageController()->completionSettings(), &ICompletionSettings::settingsChanged,
            this, &ColorCache::updateColorsFromSettings, Qt::QueuedConnection);

    connect(ICore::self()->documentController(), &IDocumentController::documentActivated,
            this, &ColorCache::slotDocumentActivated);

    bool hadDoc = tryActiveDocument();

    updateInternal();

    m_self = this;

    if (!hadDoc) {
        // try to update later on again
        QMetaObject::invokeMethod(this, "tryActiveDocument", Qt::QueuedConnection);
    }
}

bool ColorCache::tryActiveDocument()
{
    KTextEditor::View* view = ICore::self()->documentController()->activeTextDocumentView();
    if (view) {
        updateColorsFromView(view);
        return true;
    }
    return false;
}

ColorCache::~ColorCache()
{
    m_self = nullptr;
    delete m_defaultColors;
    m_defaultColors = nullptr;
}

ColorCache* ColorCache::self()
{
    if (!m_self) {
        m_self = new ColorCache;
    }
    return m_self;
}

void ColorCache::generateColors()
{
    if (!m_defaultColors) {
        m_defaultColors = new CodeHighlightingColors(this);
    }

    // Primary colors taken from: http://colorbrewer2.org/?type=qualitative&scheme=Paired&n=12
    const QColor colors[] = {
        {"#b15928"}, {"#ff7f00"}, {"#b2df8a"}, {"#33a02c"}, {"#a6cee3"},
        {"#1f78b4"}, {"#6a3d9a"}, {"#cab2d6"}, {"#e31a1c"}, {"#fb9a99"}
    };
    const int colorCount = std::extent<decltype(colors)>::value;

    // Supplementary colors generated by: http://tools.medialab.sciences-po.fr/iwanthue/
    const QColor supplementaryColors[] = {
        {"#D33B67"}, {"#5EC764"}, {"#6CC82D"}, {"#995729"}, {"#FB4D84"},
        {"#4B8828"}, {"#D847D0"}, {"#B56AC5"}, {"#E96F0C"}, {"#DC7161"},
        {"#4D7279"}, {"#01AAF1"}, {"#D2A237"}, {"#F08CA5"}, {"#C83E93"},
        {"#5D7DF7"}, {"#EFBB51"}, {"#108BBB"}, {"#5C84B8"}, {"#02F8BC"},
        {"#A5A9F7"}, {"#F28E64"}, {"#A461E6"}, {"#6372D3"}
    };
    const int supplementaryColorCount = std::extent<decltype(supplementaryColors)>::value;

    m_colors.clear();
    m_colors.reserve(colorCount + supplementaryColorCount);

    for (const auto& color: colors) {
        m_colors.append(blendLocalColor(color));
    }

    m_primaryColorCount = m_colors.count();

    for (const auto& color: supplementaryColors) {
        m_colors.append(blendLocalColor(color));
    }

    m_validColorCount = m_colors.count();
}

void ColorCache::slotDocumentActivated()
{
    KTextEditor::View* view = ICore::self()->documentController()->activeTextDocumentView();
    ifDebug(qCDebug(LANGUAGE) << "doc activated:" << doc; )
    if (view) {
        updateColorsFromView(view);
    }
}

void ColorCache::slotViewSettingsChanged()
{
    auto* view = qobject_cast<KTextEditor::View*>(sender());
    Q_ASSERT(view);

    ifDebug(qCDebug(LANGUAGE) << "settings changed" << view; )
    updateColorsFromView(view);
}

void ColorCache::updateColorsFromView(KTextEditor::View* view)
{
    if (!view) {
        // yeah, the HighlightInterface methods returning an Attribute
        // require a View... kill me for that mess
        return;
    }

    QColor foreground(QColor::Invalid);
    QColor background(QColor::Invalid);

    KTextEditor::Attribute::Ptr style = view->defaultStyleAttribute(KTextEditor::dsNormal);
    foreground = style->foreground().color();
    if (style->hasProperty(QTextFormat::BackgroundBrush)) {
        background = style->background().color();
    }

    // FIXME: this is in kateview
//     qCDebug(LANGUAGE) << "got foreground:" << foreground.name() << "old is:" << m_foregroundColor.name();
//NOTE: this slot is defined in KatePart > 4.4, see ApiDocs of the ConfigInterface

    // the signal is not defined in ConfigInterface, but according to the docs it should be
    // can't use new signal slot syntax here, since ConfigInterface is not a QObject
    if (KTextEditor::View* view = m_view.data()) {
        Q_ASSERT(qobject_cast<KTextEditor::ConfigInterface*>(view));
        // we only listen to a single view, i.e. the active one
#if KTEXTEDITOR_VERSION >= QT_VERSION_CHECK(5, 79, 0)
        disconnect(view, &KTextEditor::View::configChanged, this, &ColorCache::slotViewSettingsChanged);
#else
        disconnect(view, SIGNAL(configChanged()), this, SLOT(slotViewSettingsChanged()));
#endif
    }
    Q_ASSERT(qobject_cast<KTextEditor::ConfigInterface*>(view));
#if KTEXTEDITOR_VERSION >= QT_VERSION_CHECK(5, 79, 0)
    connect(view, &KTextEditor::View::configChanged, this, &ColorCache::slotViewSettingsChanged);
#else
    connect(view, SIGNAL(configChanged()), this, SLOT(slotViewSettingsChanged()));
#endif
    m_view = view;

    bool anyAttrChanged = false;
    if (!foreground.isValid()) {
        // fallback to colorscheme variant
        ifDebug(qCDebug(LANGUAGE) << "updating from scheme"; )
        updateColorsFromScheme();
    } else if (m_foregroundColor != foreground || m_backgroundColor != background) {
        m_foregroundColor = foreground;
        m_backgroundColor = background;
        anyAttrChanged = true;
    }

    anyAttrChanged |= updateColorsFromTheme(view->theme());

    if (anyAttrChanged) {
        ifDebug(qCDebug(LANGUAGE) << "updating from document"; )
        update();
    }
}

bool ColorCache::updateColorsFromTheme(const KSyntaxHighlighting::Theme& theme)
{
    // from ktexteditor/src/syntax/kateextendedattribute.h
    static const int SelectedBackground = QTextFormat::UserProperty + 2;

    const auto schemeDefinition = m_schemeRepo.definitionForName(QStringLiteral("Semantic Colors"));
    const auto formats = schemeDefinition.formats();
    const bool blendColors = m_globalColorRatio < 255;
    bool anyAttrChanged = false;
    for (const auto& format : formats) {
        const auto type = getHighlightingTypeFromName(format.name());
        const auto attr = m_defaultColors->attribute(type);

        auto fg = format.textColor(theme);
        auto selFg = format.selectedTextColor(theme);
        if (blendColors) {
            fg = blendGlobalColor(fg);
            selFg = blendGlobalColor(selFg);
        }
        if (attr->fontBold() != format.isBold(theme)) {
            attr->setFontBold(format.isBold(theme));
            anyAttrChanged = true;
        }
        if (attr->fontItalic() != format.isItalic(theme)) {
            attr->setFontItalic(format.isItalic(theme));
            anyAttrChanged = true;
        }
        if (attr->fontUnderline() != format.isUnderline(theme)) {
            attr->setFontUnderline(format.isUnderline(theme));
            anyAttrChanged = true;
        }
        if (attr->fontStrikeOut() != format.isStrikeThrough(theme)) {
            attr->setFontStrikeOut(format.isStrikeThrough(theme));
            anyAttrChanged = true;
        }
        if (attr->foreground().color() != fg) {
            attr->setForeground(fg);
            anyAttrChanged = true;
        }
        if (attr->selectedForeground().color() != selFg) {
            attr->setSelectedForeground(selFg);
            anyAttrChanged = true;
        }
        if (format.hasBackgroundColor(theme)) {
            if (attr->background().color() != format.backgroundColor(theme)) {
                attr->setBackground(format.backgroundColor(theme));
                anyAttrChanged = true;
            }
        } else if (type == CodeHighlightingInstance::HighlightUsesType ) {
            auto background = QColor(theme.editorColor(KSyntaxHighlighting::Theme::SearchHighlight));
            if (attr->background().color() != background) {
                attr->setBackground(background);
                anyAttrChanged = true;
            }
        } else if (attr->background() != QBrush()) {
            attr->setBackground(QBrush());
            anyAttrChanged = true;
        }
        // from KSyntaxHighlighting::Format::isDefaultTextStyle
        if (format.selectedBackgroundColor(theme).rgba() != theme.selectedBackgroundColor(KSyntaxHighlighting::Theme::Normal)) {
            if (attr->selectedBackground().color() != format.selectedBackgroundColor(theme)) {
                attr->setSelectedBackground(format.selectedBackgroundColor(theme));
                anyAttrChanged = true;
            }
        } else if (attr->hasProperty(SelectedBackground)) {
            attr->clearProperty(SelectedBackground);
            anyAttrChanged = true;
        }
    }
    return anyAttrChanged;
}

void ColorCache::updateColorsFromScheme()
{
    KColorScheme scheme(QPalette::Normal, KColorScheme::View);

    QColor foreground = scheme.foreground(KColorScheme::NormalText).color();
    QColor background = scheme.background(KColorScheme::NormalBackground).color();

    if (foreground != m_foregroundColor || background != m_backgroundColor) {
        m_foregroundColor = foreground;
        m_backgroundColor = background;
        update();
    }
}

void ColorCache::updateColorsFromSettings()
{
    int localRatio = ICore::self()->languageController()->completionSettings()->localColorizationLevel();
    int globalRatio = ICore::self()->languageController()->completionSettings()->globalColorizationLevel();
    bool boldDeclartions = ICore::self()->languageController()->completionSettings()->boldDeclarations();
    bool globalRatioChanged = globalRatio != m_globalColorRatio;

    if (localRatio != m_localColorRatio || globalRatioChanged) {
        m_localColorRatio = localRatio;
        m_globalColorRatio = globalRatio;
        if (m_view && globalRatioChanged) {
            updateColorsFromTheme(m_view->theme());
        }
        update();
    }
    if (boldDeclartions != m_boldDeclarations) {
        m_boldDeclarations = boldDeclartions;
        update();
    }
}

void ColorCache::update()
{
    if (!m_self) {
        ifDebug(qCDebug(LANGUAGE) << "not updating - still initializating"; )
        // don't update on startup, updateInternal is called directly there
        return;
    }

    QMetaObject::invokeMethod(this, "updateInternal", Qt::QueuedConnection);
}

void ColorCache::updateInternal()
{
    ifDebug(qCDebug(LANGUAGE) << "update internal" << m_self; )
    generateColors();

    if (!m_self) {
        // don't do anything else fancy on startup
        return;
    }

    emit colorsGotChanged();

    // rehighlight open documents
    if (!ICore::self() || ICore::self()->shuttingDown()) {
        return;
    }
    const auto documents = ICore::self()->documentController()->openDocuments();
    for (IDocument* doc : documents) {
        const auto languages = ICore::self()->languageController()->languagesForUrl(doc->url());
        for (const auto lang : languages) {
            ReferencedTopDUContext top;
            {
                DUChainReadLocker lock;
                top = lang->standardContext(doc->url());
            }

            if (top) {
                if (ICodeHighlighting* highlighting = lang->codeHighlighting()) {
                    highlighting->highlightDUChain(top);
                }
            }
        }
    }
}

QColor ColorCache::blend(QColor color, uchar ratio) const
{
    Q_ASSERT(m_backgroundColor.isValid());
    Q_ASSERT(m_foregroundColor.isValid());
    return WidgetColorizer::blendForeground(color, float( ratio ) / float( 0xff ), m_foregroundColor,
                                            m_backgroundColor);
}

QColor ColorCache::blendBackground(QColor color, uchar ratio) const
{
    return WidgetColorizer::blendBackground(color, float( ratio ) / float( 0xff ), m_foregroundColor,
                                            m_backgroundColor);
}

QColor ColorCache::blendGlobalColor(QColor color) const
{
    return blend(color, m_globalColorRatio);
}

QColor ColorCache::blendLocalColor(QColor color) const
{
    return blend(color, m_localColorRatio);
}

CodeHighlightingColors* ColorCache::defaultColors() const
{
    Q_ASSERT(m_defaultColors);
    return m_defaultColors;
}

QColor ColorCache::generatedColor(uint num) const
{
    return num > ( uint )m_colors.size() ? foregroundColor() : m_colors[num];
}

uint ColorCache::validColorCount() const
{
    return m_validColorCount;
}

uint ColorCache::primaryColorCount() const
{
    return m_primaryColorCount;
}

QColor ColorCache::foregroundColor() const
{
    return m_foregroundColor;
}
}
