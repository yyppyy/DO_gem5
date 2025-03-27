## Build dependencies
$sudo apt-get install bison

$sudo apt-get install byacc

$sudo apt-get install flex

## General
CMurphi (Caching Murphi) is a model checker developed on the top of Murphi.

The original Murphi web page is [archived here](https://web.archive.org/web/20140406081619/http://verify.stanford.edu/dill/murphi.html) (Stanford University), while a more up-to-date page is [here](https://formalverification.cs.utah.edu/Murphi/) (University of Utah).

CMurphi implements two new model checking algorithms. The first one has been obtained from Murphi 3.1 release by replacing Murphi Hash Table with a Cache and Murphi RAM queue with a disk queue. The second one is disk based. Both are described in [STTT 2004](http://mclab.di.uniroma1.it/publications/show.php?record=91).

The current version of CMurphi is a refactored one:

- it corrects some errors coming out with newer versions of g++
- it supports 64-bit architectures also when using hash compaction
- it includes [FHP-Murphi](http://mclab.di.uniroma1.it/site/index.php/software/17-fhp-murphi)

## License

CMurphi is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
of the License, or (at your option) any later version.

CMurphi is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA.

To contact the CMurphi development board, email to <melatti@di.uniroma1.it>.
Copyright (C) 2009--2012 by Sapienza University of Rome.

## History

CMurphi heavily lies on the Murphi model checker by Stanford. Many errors have
been corrected, but some could still be there (especially in symmetry
reduction). Moreover, the software architecture is not the best one, since
CMurphi has evolved in order to be as compatible as possible with the original
Murphi, rather than employing the best C++ features.

Starting from version CMurphi 5.4, a major refactoring has been done. Version
history starting from CMurphi 5.4 is the following:

* 5.4.1: 64 bits for caching, fix
* 5.4.2: some Intel warnings caught
* 5.4.3: fixings to compile on Cygwin
* 5.4.4: fixed `-d` option
* 5.4.5: option `--noht` added, counterexample in DFS by reading the stack, some
         errors fixed
* 5.4.6: option `--trace-dfs` added: counterexample in DFS by reading the stack also
         available with hashtable
* 5.4.7: corrected an error in disk algorithm
* 5.4.8: corrected an error in simulation error tracing (thanks to Mark R. Tuttle)
* 5.4.9: corrected an error when handling both `--noht` and `--trace-dfs`
* 5.4.9.1: corrected an old error (back to Murphi) on `cpp_code.cpp` (thanks to
         Matthew Fernandez)

The original Murphi license, also mentioning the original Murphi authors, is
included in [MURPHI.md](MURPHI.md).
