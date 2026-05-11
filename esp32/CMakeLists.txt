cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS components)

# ESP-IDF compile TOUS les composants enregistres, pas seulement ceux dans l'arbre
# de dependances de main. bluepad32 depend de sources btstack absentes du depot;
# EXCLUDE_COMPONENTS l'exclut sans toucher au composant interne de PlatformIO.
set(EXCLUDE_COMPONENTS bluepad32)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(spiderbot_esp32)
