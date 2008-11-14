/* KDevelop CMake Support
 *
 * Copyright 2008 Aleix Pol Gonzalez <aleixpol@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "cmakeduchaintest.h"
#include "cmakeprojectvisitor.h"

#include <language/duchain/identifier.h>
#include <language/duchain/declaration.h>
#include <language/duchain/duchainlock.h>
#include <language/duchain/duchain.h>
#include <language/editor/simplerange.h>
#include <language/duchain/dumpchain.h>
#include <language/duchain/use.h>
#include <language/duchain/indexedstring.h>

using namespace KDevelop;

QTEST_MAIN( CMakeDUChainTest )

Q_DECLARE_METATYPE(QList<SimpleRange>)

CMakeDUChainTest::CMakeDUChainTest()
{
	DUChainWriteLocker lock(DUChain::lock());
	m_fakeContext = new TopDUContext(IndexedString("test"), SimpleRange(0,0,0,0));
}

CMakeDUChainTest::~CMakeDUChainTest()
{}

void CMakeDUChainTest::testDUChainWalk_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QList<SimpleRange> >("ranges");

    QTest::newRow("simple") << "project(simpletest)\n" << QList<SimpleRange>();

    QList<SimpleRange> sr;
    sr.append(SimpleRange(1, 4, 1, 7));
    QTest::newRow("simple 2") <<
            "project(simpletest)\n"
            "set(var a b c)\n" << sr;

    QTest::newRow("simple 3") <<
            "project(simpletest)\n"
            "find_package(KDE4)\n" << QList<SimpleRange>();


    sr.append(SimpleRange(2, 4, 2, 8));
    QTest::newRow("simple 2 with use") <<
            "project(simpletest)\n"
            "set(var a b c)\n"
	    "set(var2 ${var})\n"<< sr;

    sr.clear();
    sr.append(SimpleRange(1, 15, 1, 18));
    QTest::newRow("simple 2 with use") <<
            "project(simpletest)\n"
            "add_executable(var a b c)\n" << sr;
}

void CMakeDUChainTest::testDUChainWalk()
{
    QFETCH(QString, input);
    QFETCH(QList<SimpleRange>, ranges);

    QFile file("cmake_duchain_test");
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QTextStream out(&file);
    out << input;
    file.close();
    CMakeFileContent code=CMakeListsParser::readCMakeFile(file.fileName());
    file.remove();
    QVERIFY(code.count() != 0);

    MacroMap mm;
    VariableMap vm;

    CMakeProjectVisitor v(file.fileName(), m_fakeContext);
    v.setVariableMap(&vm);
    v.setMacroMap(&mm);
//     v.setModulePath();
    v.walk(code, 0);

    DUChainWriteLocker lock(DUChain::lock());
    TopDUContext* ctx=v.context();
    QVERIFY(ctx);

    QVector<Declaration*> declarations=ctx->localDeclarations();
    /*for(int i=0; i<declarations.count(); i++)
    {
        qDebug() << "ddd" << declarations[i]->identifier().toString();
        if(!ranges.contains(declarations[i]->range()))
            qDebug() << "doesn't exist " << declarations[i]->range().start.column
                << declarations[i]->range().end.column;
        QVERIFY(ranges.contains(declarations[i]->range()));
    }*/

    foreach(const SimpleRange& sr, ranges)
    {
        bool found=false;
        for(int i=0; !found && i<declarations.count(); i++)
        {
            if(declarations[i]->range()==sr)
                found=true;
            else
                qDebug() << "diff " << declarations[i]->range().start.column << declarations[i]->range().end.column
                    << declarations[i]->range().end.line;
        }
        if(!found)
            qDebug() << "doesn't exist " << sr.start.column << sr.end.column;
        QVERIFY(found);
    }
}

void CMakeDUChainTest::testUses()
{
    QString input(
            "project(simpletest)\n"
            "set(var a b c)\n"
            "set(var2 ${var})\n"
            
            "macro(bla kk)\n"
            " message(STATUS ${kk})\n"
            "endmacro(bla)\n"
//             "bla(kk)\n"
            );

    QFile file("cmake_duchain_test");
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QTextStream out(&file);
    out << input;
    file.close();
    CMakeFileContent code=CMakeListsParser::readCMakeFile(file.fileName());
    file.remove();
    QVERIFY(code.count() != 0);

    MacroMap mm;
    VariableMap vm;
    
    m_fakeContext->setRange(SimpleRange(0,0, 2,31));

    CMakeProjectVisitor v(file.fileName(), m_fakeContext);
    v.setVariableMap(&vm);
    v.setMacroMap(&mm);
    v.walk(code, 0);

    DUChainWriteLocker lock(DUChain::lock());
    TopDUContext* ctx=v.context();
    QVERIFY(ctx);
    QVERIFY(ctx->indexed().data());
    KDevelop::DumpChain dump;
    dump.dump(ctx);
    QVector<Declaration*> declarations=ctx->localDeclarations();
    QCOMPARE(ctx->range().start.line, 0);
    
    QStringList decls=QStringList() << "var" << "var2" << "bla";
    QCOMPARE(decls.count(), declarations.count());
    for(int i=0; i<decls.count(); i++) {
        QVERIFY(declarations[i]->inSymbolTable());
    }
    
    for(int i=0; i<decls.count(); i++)
    {
        QCOMPARE(decls[i], declarations[i]->identifier().toString());
        QCOMPARE(1, ctx->findLocalDeclarations(Identifier(decls[i])).count());
    }
    
    
    kDebug() << declarations[0]->identifier().toString();
    kDebug() << ctx->range().textRange() << declarations[0]->range().textRange();
    QCOMPARE(ctx->findLocalDeclarations(Identifier("var")).count(), 1);
    QCOMPARE(ctx->findDeclarations(Identifier("var")).count(), 1);
    
    QCOMPARE(ctx->usesCount(), 2);
    
    KTextEditor::Range sr=ctx->uses()[0].m_range.textRange();
    if(sr != SimpleRange(2,11, 2,11+3).textRange()) {
        kDebug() << "wrong range" << sr;
        
        for(int i=0; i<ctx->usesCount(); i++)
        {
            kDebug() << "use " << i << ctx->uses()[i].m_range.textRange();
        }
    }
    QCOMPARE(sr, SimpleRange(2,11, 2,11+3).textRange());

//     QCOMPARE(ctx->range().end.column, 15);
//     QCOMPARE(ctx->range().end.line, 2);
}

#include "cmakeduchaintest.moc"

