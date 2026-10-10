# Project Ambrose by Imjustchico
# Builds the inventory library: a wizard's backpack with its capacity and the rules for adding, removing and trashing items, the one line every item's global id is handed out from, the game object an item is shown to the client as, and the slots a wizard wears items in with the rules that equip and unequip them, judged by the requirement evaluator.
ambrose_add_library(inventory "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(inventory
    PUBLIC
        characters
        entities
        items
        conditions
        globals)
