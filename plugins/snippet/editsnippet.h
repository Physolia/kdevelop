/***************************************************************************
 *   Copyright (C) 2007 by Robert Gruber                                   *
 *   rgruber@users.sourceforge.net                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef __EDITSNIPPET_H__
#define __EDITSNIPPET_H__


#include "ui_editsnippet.h"

class Snippet;

/**
 * A dialog that allows to edit the given snippet
 * @see editsnippet.ui
 * @author Robert Gruber <rgruber@users.sourceforge.net>
 */
class EditSnippet : public QDialog, public Ui::EditSnippetBase
{
    Q_OBJECT

public:
    EditSnippet(Snippet *s, QWidget *parent = 0);
    virtual ~EditSnippet();


public slots:
    virtual void accept();

private:
    void fillWithItemsFromListWidget(QStringList& list);

    Snippet *snippet_;
};

#endif

// kate: space-indent on; indent-width 4; tab-width 4; indent-mode cstyle; replace-tabs on; auto-insert-doxygen on;
