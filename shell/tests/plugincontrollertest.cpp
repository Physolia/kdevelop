/***************************************************************************
 *   Copyright 2008 Andreas Pakulat <apaku@gmx.de>                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/

#include "plugincontrollertest.h"

#include <qtest_kde.h>
#include <tests/common/autotestshell.h>

#include "../core.h"
#include "../plugincontroller.h"

using KDevelop::Core;
using KDevelop::PluginController;

using QTest::kWaitForSignal;

////////////////////// Fixture ///////////////////////////////////////////////

void PluginControllerTest::initTestCase()
{
    AutoTestShell::init();
    Core::initialize( KDevelop::Core::NoUi );
    m_core = Core::self();
    m_pluginCtrl = m_core->pluginControllerInternal();
}

void PluginControllerTest::init()
{
}

void PluginControllerTest::cleanup()
{
}

void PluginControllerTest::loadPlugin()
{
    m_pluginCtrl->loadPlugin( "KDevStandardOutputView" );
    QVERIFY( m_pluginCtrl->plugin( "KDevStandardOutputView" ) );
}

QTEST_KDEMAIN( PluginControllerTest, GUI)
#include "plugincontrollertest.moc"
