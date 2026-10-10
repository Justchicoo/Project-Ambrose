# Project Ambrose by Imjustchico
# Builds the npc library: the services an NPC offers the wizards around it, the flat option index a client sends back mapped to the provider that owns it, the range each wizard enters and leaves once per crossing, and the ServiceMementoBase blob MSG_SENDNPCOPTIONS carries.
ambrose_add_library(npc "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(npc
    PUBLIC
        quests
        scripting)
