/*
 * opencog/learning/PatternMiner/PatternMiner.h
 *
 * Copyright (C) 2012 by OpenCog Foundation
 * All Rights Reserved
 *
 * Written by Shujing Ke <rainkekekeke@gmail.com> Scott Jones <troy.scott.j@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _OPENCOG_PATTERNMINER_PATTERNMINER_H
#define _OPENCOG_PATTERNMINER_PATTERNMINER_H
#include <map>
#include <vector>
#include "Pattern.h"
#include "HTree.h"
#include <opencog/atomspace/AtomSpace.h>

using namespace std;

namespace opencog
{
 namespace PatternMiner
{

 class PatternMiner
 {
private:
    HTree* htree;
    AtomSpace* atomSpace;
public:
    PatternMiner(AtomSpace* _atomSpace): atomSpace(_atomSpace)
    {
        htree = new HTree();
    }

    void processOneInputStream(HandleSeq inputSteam);

 };

}
}

#endif //_OPENCOG_PATTERNMINER_PATTERNMINER_H
