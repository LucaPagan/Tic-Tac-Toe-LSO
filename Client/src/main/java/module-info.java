module tristrick.client {
    requires javafx.controls;
    requires javafx.fxml;
    requires org.controlsfx.controls;
    requires com.google.gson;
    requires javafx.graphics;

    // Esposizione perimetrale verso JVM e framework UI
    opens tristrick.client to javafx.fxml;
    opens tristrick.client.util to com.google.gson, javafx.fxml; // Mapping riflessivo per instradamento payload

    exports tristrick.client;
    exports tristrick.client.util;
    exports tristrick.client.gui;

    opens tristrick.client.gui to javafx.fxml; // Dispatch FXML controller
}