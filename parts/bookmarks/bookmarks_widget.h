/***************************************************************************
 *   Copyright (C) 2003 by Jens Dagerbo                                    *
 *   jens.dagerbo@swipnet.se                                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef __BOOKMARKS_WIDGET_H__
#define __BOOKMARKS_WIDGET_H__


#include <qwidget.h>
#include <qstring.h>
#include <qlistview.h>
#include <qtooltip.h>

#include "bookmarks_part.h"

class BookmarksPart;
class BookmarkItem;

class BookmarksWidget : public QListView, public QToolTip
{
	Q_OBJECT

public:
	BookmarksWidget( BookmarksPart * );
	~BookmarksWidget();

	void update( QDict<EditorData> const & );
	void updateURL( EditorData * );
	bool removeURL( KURL const & );

signals:
	void removeAllBookmarksForURL( const KURL & );
	void removeBookmarkForURL( const KURL &, int );

protected:
	void maybeTip( QPoint const & );

private slots:
	void itemClicked( QListViewItem * );
	void popupMenu( QListViewItem * , const QPoint & , int );
	void doEmitRemoveBookMark();

private:
	void createURL( EditorData * );

	BookmarksPart * _part;
	BookmarkItem * _selectedItem;
	
	friend class BookmarkItem;
};


#endif
