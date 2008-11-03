/* This file is part of KDevelop
*  Copyright (C) 2008 Cédric Pasteur <cedric.pasteur@free.fr>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
 */

#include "indent_plugin.h"

#include <KPluginLoader>
#include <KPluginFactory>
#include <KAboutData>
#include <QTextStream>
#include <KDebug>
#include <KProcess>

using namespace KDevelop;

K_PLUGIN_FACTORY(IndentFactory, registerPlugin<IndentPlugin>();)
K_EXPORT_PLUGIN(IndentFactory(KAboutData("kdevindent","kdevindent", ki18n("Indent Formatter"), "0.1", ki18n("A formatter using indent"), KAboutData::License_GPL)))

IndentPlugin::IndentPlugin(QObject *parent, const QVariantList&)
		: IPlugin(IndentFactory::componentData(), parent)
{
	KDEV_USE_EXTENSION_INTERFACE(ISourceFormatter)
}

IndentPlugin::~IndentPlugin()
{
}

QString IndentPlugin::name()
{
	return "indent";
}

QString IndentPlugin::caption()
{
	return "GNU Indent";
}

QString IndentPlugin::description()
{
	// check if indent is installed
	KProcess proc;
	QStringList args;
	args << "--version";
	proc.setProgram("indent", args);
	int res = proc.execute();

	if(res >= 0) //indent was found
		return i18n("<b>Indent and Format C Program Source.</b><br />"
		"The `indent' program can be used to make code easier to read."
		" It can also convert from one style of writing C to another.<br />"
		"<b>indent</b> understands a substantial amount about the syntax of C,"
		" but it also attempts to cope with incomplete and misformed syntax.<br />"
		"Home Page: <a href=\"http://www.gnu.org/software/indent/\">"
		"http://www.gnu.org/software/indent</a>/");
	else
		return ISourceFormatter::missingExecutableMessage("indent");
}

QString IndentPlugin::highlightModeForMime(const KMimeType::Ptr &mime)
{
	return "C++";
}

QString IndentPlugin::formatSource(const QString &text, const KMimeType::Ptr &)
{
	KProcess proc;
	QTextStream ios(&proc);
	proc.setProgram("indent", m_options);
	proc.setOutputChannelMode(KProcess::MergedChannels);

	proc.start();
	if(!proc.waitForStarted()) {
		kDebug() << "Unable to start indent" << endl;
		return text;
	}

	proc.write(text.toLocal8Bit());
	proc.closeWriteChannel();
	if(!proc.waitForFinished()) {
		kDebug() << "Process doesn't finish" << endl;
		return text;
	}

	QString output = ios.readAll();
	return output;
}

QMap<QString, QString> IndentPlugin::predefinedStyles(const KMimeType::Ptr &)
{
	QMap<QString, QString> styles;
	styles.insert("orig", "Original Berkeley indent style");
	styles.insert("GNU", "GNU");
	styles.insert("KR", "Kernighan & Ritchie");

	return styles;
}

void IndentPlugin::setStyle(const QString &name, const QString &content)
{
	if (!name.isEmpty()) {
		m_options.clear();
		if(name == "KR")
			m_options << "-kr";
		else if(name == "orig")
			m_options << "-orig";
	}
	else
		m_options = content.split(' ');
}

KDevelop::SettingsWidget* IndentPlugin::editStyleWidget(const KMimeType::Ptr &mime)
{
// 	return new IndentPreferences();
	return 0;
}

QString IndentPlugin::previewText(const KMimeType::Ptr &)
{
	return "int foo (){puts(\"Hi\");}\n/* The procedure bar is even less interesting.  */\n"
	"char * bar () { puts(\"Hello\");}";
}

ISourceFormatter::IndentationType IndentPlugin::indentationType()
{
	if(m_options.contains("-nut"))
		return ISourceFormatter::IndentWithSpaces;
	else
		return ISourceFormatter::IndentWithTabs;
}

int IndentPlugin::indentationLength()
{
	int idx = m_options.indexOf("^-i\\d+");
	if(idx < 0)
		return 2;
	return m_options[idx].mid(2).toInt();
}

#include "indent_plugin.moc"

// kate: indent-mode cstyle; space-indent off; tab-width 4;
