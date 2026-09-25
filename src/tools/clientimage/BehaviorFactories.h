/*
 * Project Ambrose by Imjustchico
 * Finds the class a client program builds for each behavior it registers: the program reads each behavior's name and stores a factory for it, whose create function makes an object and gives it the vtable of one class, and that class's GetType registers its name under the type its parent's GetType returns, so the name read, the factory, the object's vtable, the class name and its bases are followed through the code index to the class the client really makes.
 */

#ifndef AMBROSE_BEHAVIORFACTORIES_H
#define AMBROSE_BEHAVIORFACTORIES_H

#include "Types.h"

#include <string>
#include <string_view>
#include <vector>

class CodeIndex;
class PeImage;

struct BehaviorFactory
{
    std::string Behavior;
    uint64 Site = 0;
    uint64 FactoryVtable = 0;
    uint64 Create = 0;
    uint64 ObjectVtable = 0;
    uint64 GetType = 0;
    std::string ClassName;
    std::vector<std::string> Bases;
};

namespace BehaviorFactories
{
    bool IsBehaviorName(std::string_view text);
    std::vector<BehaviorFactory> Find(PeImage const& image, CodeIndex const& index);
}

#endif
