#include "astyle_adaptor.h"

#include "astyle_widget.h"

#include <string>

#include <qradiobutton.h>
#include <qspinbox.h>
#include <qcheckbox.h>

#include <kapplication.h>
#include <kconfig.h>



ASStringIterator::ASStringIterator(const QString &text)
  : ASSourceIterator(), _content(text)
{
  _is = new QTextStream(&_content, IO_ReadOnly);
}


ASStringIterator::~ASStringIterator()
{
  delete _is;
}


bool ASStringIterator::hasMoreLines() const
{
  return !_is->eof();
}


string ASStringIterator::nextLine()
{
  return _is->readLine().utf8().data();
}




KDevFormatter::KDevFormatter()
{
	KConfig *config = kapp->config();
	config->setGroup("AStyle");

	// style
	QString s = config->readEntry("Style");

	if ( predefinedStyle( s ) )
	{
		return;
	}

  // fill
	int wsCount = config->readNumEntry("FillCount",2);
  if (config->readEntry("Fill", "Tabs") == "Tabs")
  {
	  setTabIndentation(wsCount, config->readBoolEntry("FillForce",false) );
	  m_indentString = "\t";
  } else
  {
	  setSpaceIndentation(wsCount);
	  m_indentString = "";
	  m_indentString.fill(' ', wsCount);
	  setTabSpaceConversionMode(config->readBoolEntry("FillForce",true));
  }

  // indent
  setSwitchIndent(config->readBoolEntry("IndentSwitches", false));
  setClassIndent(config->readBoolEntry("IndentClasses", false));
  setCaseIndent(config->readBoolEntry("IndentCases", false));
  setBracketIndent(config->readBoolEntry("IndentBrackets", false));
  setNamespaceIndent(config->readBoolEntry("IndentNamespaces", true));
  setLabelIndent(config->readBoolEntry("IndentLabels", true));

  // continuation
  setMaxInStatementIndentLength(config->readNumEntry("MaxStatement", 40));
  if (config->readNumEntry("MinConditional", -1) != -1)
    setMinConditionalIndentLength(config->readNumEntry("MinConditional"));

  // brackets
  s = config->readEntry("Brackets", "None");
  if (s == "Break")
	  setBracketFormatMode(astyle::BREAK_MODE);
  else if (s == "Attach")
	  setBracketFormatMode(astyle::ATTACH_MODE);
  else if (s == "Linux")
	  setBracketFormatMode(astyle::BDAC_MODE);
  else
	  setBracketFormatMode(astyle::NONE_MODE);

  // padding
  setOperatorPaddingMode(config->readBoolEntry("PadOperators", false));
  setParensInsidePaddingMode(config->readBoolEntry("PadOperatorsIn", false));
  setParensOutsidePaddingMode(config->readBoolEntry("PadOperatorsOut", false));
  setParensUnPaddingMode(config->readBoolEntry("PadOperatorsUn", false));

  // oneliner
  setBreakOneLineBlocksMode(config->readBoolEntry("KeepBlocks", false));
  setSingleStatementsMode(config->readBoolEntry("KeepStatements", false));
}

KDevFormatter::KDevFormatter( AStyleWidget * widget )
{
	setCStyle();

	if ( widget->Style_ANSI->isChecked() )
	{
		predefinedStyle( "ANSI" );
		return;
	}
	if ( widget->Style_GNU->isChecked() )
	{
		predefinedStyle( "GNU" );
		return;
	}
	if ( widget->Style_JAVA->isChecked() )
	{
		predefinedStyle( "JAVA" );
		return;
	}
	if ( widget->Style_KR->isChecked() )
	{
		predefinedStyle( "KR" );
		return;
	}
	if ( widget->Style_Linux->isChecked() )
	{
		predefinedStyle( "Linux" );
		return;
	}

	// fill
	if ( widget->Fill_Tabs->isChecked() )
	{
		setTabIndentation(widget->Fill_TabCount->value(), widget->Fill_ForceTabs->isChecked());
		m_indentString = "\t";
	}
	else
	{
		setSpaceIndentation( widget->Fill_SpaceCount->value() );
		m_indentString = "";
		m_indentString.fill(' ', widget->Fill_SpaceCount->value());
	}

	setTabSpaceConversionMode(widget->Fill_ConvertTabs->isChecked());
	setEmptyLineFill(widget->Fill_EmptyLines->isChecked());

	// indent
	setSwitchIndent( widget->Indent_Switches->isChecked() );
	setClassIndent( widget->Indent_Classes->isChecked() );
	setCaseIndent( widget->Indent_Cases->isChecked() );
	setBracketIndent( widget->Indent_Brackets->isChecked() );
	setNamespaceIndent( widget->Indent_Namespaces->isChecked() );
	setLabelIndent( widget->Indent_Labels->isChecked() );
	setBlockIndent( widget->Indent_Blocks->isChecked());
	setPreprocessorIndent(widget->Indent_Preprocessors->isChecked());

	// continuation
	setMaxInStatementIndentLength( widget->Continue_MaxStatement->value() );
	setMinConditionalIndentLength( widget->Continue_MinConditional->value() );

	// brackets
	if ( widget->Brackets_Break->isChecked() )
	{
		setBracketFormatMode( astyle::BREAK_MODE );
	}
	else if ( widget->Brackets_Attach->isChecked() )
	{
		setBracketFormatMode( astyle::ATTACH_MODE );
	}
	else if ( widget->Brackets_Linux->isChecked())
	{
		setBracketFormatMode( astyle::BDAC_MODE );
	}
	else{
		setBracketFormatMode( astyle::NONE_MODE );
	}

	setBreakClosingHeaderBracketsMode( widget->Brackets_CloseHeaders->isChecked());

	// blocks
	setBreakBlocksMode(widget->Block_Break->isChecked());
	if (widget->Block_BreakAll->isChecked()){
		setBreakBlocksMode(true);
		setBreakClosingHeaderBlocksMode(true);
	}
	setBreakElseIfsMode(widget->Block_IfElse->isChecked());

	// padding
	setOperatorPaddingMode( widget->Pad_Operators->isChecked() );

	setParensInsidePaddingMode( widget->Pad_ParenthesesIn->isChecked() );
	setParensOutsidePaddingMode( widget->Pad_ParenthesesOut->isChecked() );
	setParensUnPaddingMode( widget->Pad_ParenthesesUn->isChecked() );

	// oneliner
	setBreakOneLineBlocksMode( !widget->Keep_Blocks->isChecked() );
	setSingleStatementsMode( !widget->Keep_Statements->isChecked() );
}

bool KDevFormatter::predefinedStyle( const QString & style )
{
	if (style == "ANSI")
	{
		setBracketIndent(false);
		setSpaceIndentation(4);
		setBracketFormatMode(astyle::BREAK_MODE);
		setClassIndent(false);
		setSwitchIndent(false);
		setNamespaceIndent(false);
		return true;
	}
	if (style == "KR")
	{
		setBracketIndent(false);
		setSpaceIndentation(4);
		setBracketFormatMode(astyle::ATTACH_MODE);
		setClassIndent(false);
		setSwitchIndent(false);
		setNamespaceIndent(false);
		return true;
	}
	if (style == "Linux")
	{
		setBracketIndent(false);
		setSpaceIndentation(8);
		setBracketFormatMode(astyle::BDAC_MODE);
		setClassIndent(false);
		setSwitchIndent(false);
		setNamespaceIndent(false);
		return true;
	}
	if (style == "GNU")
	{
		setBlockIndent(true);
		setSpaceIndentation(2);
		setBracketFormatMode(astyle::BREAK_MODE);
		setClassIndent(false);
		setSwitchIndent(false);
		setNamespaceIndent(false);
		return true;
	}
	if (style == "JAVA")
	{
		setJavaStyle();
		setBracketIndent(false);
		setSpaceIndentation(4);
		setBracketFormatMode(astyle::ATTACH_MODE);
		setSwitchIndent(false);
		return true;
	}
	return false;
}
