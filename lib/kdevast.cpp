/* This file is part of KDevelop
Copyright (C) 2006 Adam Treat <treat@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public License
along with this library; see the file COPYING.LIB.  If not, write to
the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.
*/

#include "kdevast.h"

#include <kdebug.h>

#include "kdevlanguagesupport.h"

namespace KDevelop {

void AST::release()
{
    if (language)
        language->releaseAST(this);
    else
        kWarning() << k_funcinfo << "No language associated with AST " << this << endl;
}

void AST::documentLoaded(const KUrl& document)
{
    if (language)
        language->documentLoaded(this, document);
    else
        kWarning() << k_funcinfo << "No language associated with AST " << this << endl;
}

}
// kate: space-indent on; indent-width 4; tab-width 4; replace-tabs on
