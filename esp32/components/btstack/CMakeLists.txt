# btstack stub component - fournit les chemins d'include pour bluepad32.
# Les sources core btstack (hci.c, l2cap.c, ...) ne sont pas presentes dans
# ce depot; ce stub suffit pour le build esp32dev-sim qui n'active pas le BT.
idf_component_register(
    INCLUDE_DIRS "include" "src"
)
