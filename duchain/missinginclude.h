/*
 * Copyright 2014 Kevin Funk <kfunk@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#ifndef UNKNOWNDECLARATION_H
#define UNKNOWNDECLARATION_H

#include "clangproblem.h"

#include <language/duchain/identifier.h>

class UnknownDeclarationProblem : public KDevelop::Problem
{
public:
    using Ptr = KSharedPtr<UnknownDeclarationProblem>;
    using ConstPtr = KSharedPtr<const UnknownDeclarationProblem>;

    UnknownDeclarationProblem();

    void setSymbol(const KDevelop::QualifiedIdentifier& identifier);

    virtual KSharedPtr<KDevelop::IAssistant> solutionAssistant() const override;

private:
    KDevelop::QualifiedIdentifier m_identifier;
};

#endif // UNKNOWNDECLARATION_H
