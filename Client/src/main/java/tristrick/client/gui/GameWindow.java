package tristrick.client.gui;

import javafx.fxml.FXMLLoader;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.stage.Stage;
import tristrick.client.HomeController;

import java.io.IOException;

public class GameWindow {
    public static void open(Stage current_container, Parent hierarchy_root, HomeController hierarchy_manager,
            boolean is_primary_node) throws IOException {
        FXMLLoader UI_deployer = new FXMLLoader(GameWindow.class.getResource("/tristrick/client/game.fxml"));
        Parent viewport_root = UI_deployer.load();

        GameController operation_controller = UI_deployer.getController();
        operation_controller.setHomeRoot(hierarchy_root);
        operation_controller.setHomeController(hierarchy_manager);
        operation_controller.setIsHost(is_primary_node);

        Scene viewport_context = new Scene(viewport_root, 480, 560);
        current_container.setTitle("TRIS — Partita in Corso");
        current_container.hide();
        current_container.setScene(viewport_context);
        current_container.sizeToScene();
        current_container.centerOnScreen();
        current_container.show();
    }
}