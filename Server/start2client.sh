#/bin/bash

# echo "start server"
# ./server.out &

# echo "start first client"
# java --module-path ./lib/javafx-sdk-24.0.2/lib --add-modules javafx.controls,javafx.fxml -jar ./target/client-1.0-SNAPSHOT-shaded.jar &

# echo "start second client"
# java --module-path ./lib/javafx-sdk-24.0.2/lib --add-modules javafx.controls,javafx.fxml -jar ./target/client-1.0-SNAPSHOT-shaded.jar &

#!/bin/bash

# Primo terminale
konsole --noclose -e bash -c "cd ../Client; java --module-path ./lib/javafx-sdk-24.0.2/lib --add-modules javafx.controls,javafx.fxml -jar ./target/client-1.0-SNAPSHOT-shaded.jar" &

# Secondo terminale
konsole --noclose -e bash -c "cd ../Client; java --module-path ./lib/javafx-sdk-24.0.2/lib --add-modules javafx.controls,javafx.fxml -jar ./target/client-1.0-SNAPSHOT-shaded.jar" &

