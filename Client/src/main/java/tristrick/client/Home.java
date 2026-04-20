package tristrick.client;

import javafx.application.Application;
import javafx.fxml.FXMLLoader;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.stage.Stage;

import java.io.IOException;

public class Home extends Application {
    @Override
    public void start(Stage runtime_container) throws IOException {
        FXMLLoader UI_loader_engine = new FXMLLoader(Home.class.getResource("home.fxml"));

        Parent root_node_hierarchy = UI_loader_engine.load();

        HomeController core_state_manager = UI_loader_engine.getController();
        core_state_manager.setHomeRoot(root_node_hierarchy);

        Scene interaction_viewport = new Scene(root_node_hierarchy, 900, 640);
        interaction_viewport.getStylesheets().add(Home.class.getResource("styles.css").toExternalForm());

        runtime_container.setTitle("TRIS — Multiplayer Session Manager");
        runtime_container.setScene(interaction_viewport);
        runtime_container.show();
    }

    public static void main(String[] invocation_args) {
        launch();
    }
}