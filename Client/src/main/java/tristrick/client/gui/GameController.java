package tristrick.client.gui;

import javafx.application.Platform;
import javafx.fxml.FXML;
import javafx.scene.Parent;
import javafx.scene.control.Alert;
import javafx.scene.control.Button;
import javafx.scene.control.ButtonType;
import javafx.scene.control.Label;
import javafx.stage.Stage;
import tristrick.client.HomeController;
import tristrick.client.util.GameClient;
import tristrick.client.util.TrisGame;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.Optional;

public class GameController {

    @FXML
    private Label statusLabel;
    @FXML
    private Button cell00, cell01, cell02, cell10, cell11, cell12, cell20, cell21, cell22;

    private final Map<Integer, Button> topological_map = new HashMap<>();
    private GameClient network_gateway;
    private TrisGame simulation_engine;

    private Parent hierarchy_root;
    public HomeController hierarchy_manager;
    private boolean is_primary_node = false;

    private boolean has_token_control = false;
    private char local_glyph = 'X';
    private char external_glyph = 'O';
    @FXML
    private Button backButton;

    @FXML
    public void initialize() {
        try {
            network_gateway = GameClient.getInstance();
        } catch (IOException e) {
            broadcastTelemetry("Impossibile comunicare col server: " + e.getMessage());
            revealFallbackTrigger();
            return;
        }

        topological_map.put(1, cell00);
        topological_map.put(2, cell01);
        topological_map.put(3, cell02);
        topological_map.put(4, cell10);
        topological_map.put(5, cell11);
        topological_map.put(6, cell12);
        topological_map.put(7, cell20);
        topological_map.put(8, cell21);
        topological_map.put(9, cell22);

        cell00.setOnAction(e -> dispatchDelta(1));
        cell01.setOnAction(e -> dispatchDelta(2));
        cell02.setOnAction(e -> dispatchDelta(3));
        cell10.setOnAction(e -> dispatchDelta(4));
        cell11.setOnAction(e -> dispatchDelta(5));
        cell12.setOnAction(e -> dispatchDelta(6));
        cell20.setOnAction(e -> dispatchDelta(7));
        cell21.setOnAction(e -> dispatchDelta(8));
        cell22.setOnAction(e -> dispatchDelta(9));

        simulation_engine = new TrisGame();

        initializeDaemonHook();
    }

    public void setHomeRoot(Parent domRoot) {
        this.hierarchy_root = domRoot;
    }

    public void setIsHost(boolean primary_flag) {
        this.is_primary_node = primary_flag;
    }

    public void setHomeController(HomeController manager_ref) {
        this.hierarchy_manager = manager_ref;
    }

    @FXML
    private void handleBackButton() {
        Platform.runLater(() -> {
            Stage runtime_container = (Stage) backButton.getScene().getWindow();
            runtime_container.hide();
            runtime_container.setScene(hierarchy_root.getScene());
            runtime_container.sizeToScene();
            runtime_container.centerOnScreen();
            runtime_container.show();

            if (is_primary_node && hierarchy_manager != null) {
                hierarchy_manager.generateListenGameThread();
            }
        });
    }

    private void revealFallbackTrigger() {
        Platform.runLater(() -> {
            backButton.setVisible(true);
        });
    }

    private void promptForSessionRefresh() {
        Alert consensus_prompt = new Alert(Alert.AlertType.CONFIRMATION);
        consensus_prompt.getDialogPane().getStylesheets().add(getClass().getResource("/tristrick/client/styles.css").toExternalForm());
        consensus_prompt.setTitle("Partita Finita");
        consensus_prompt.setHeaderText("La partita è giunta al termine.");
        consensus_prompt.setContentText("Vuoi chiedere la rivincita?");

        Optional<ButtonType> local_consensus = consensus_prompt.showAndWait();

        if (local_consensus.isPresent() && local_consensus.get() == ButtonType.OK) {
            boolean external_consensus = network_gateway.rivincita(true);

            if (external_consensus) {
                Platform.runLater(() -> {
                    purgeTopologicalGrid();
                    broadcastTelemetry("Rivincita accettata! Nuova partita in corso...");
                    initializeDaemonHook();
                });
            } else {
                broadcastTelemetry("L'avversario ha rifiutato la rivincita.");
            }

        } else {
            network_gateway.rivincita(false);
            broadcastTelemetry("Hai rifiutato la rivincita.");
        }
    }

    private void purgeTopologicalGrid() {
        for (Button trigger : topological_map.values()) {
            trigger.setText("");
            trigger.setDisable(false);
            trigger.getStyleClass().removeAll("x-symbol", "o-symbol");
        }
        has_token_control = false;
        backButton.setVisible(false);
    }

    private void dispatchDelta(int physical_index) {
        if (!has_token_control) {
            broadcastTelemetry("Non è il tuo turno!");
            return;
        }
        try {
            int validated_index = simulation_engine.makeMoveUI(physical_index);
            mutateCellState(validated_index, true);
            network_gateway.sendMove(validated_index);
            has_token_control = false;
            broadcastTelemetry("Mossa inviata. Attesa risposta...");

        } catch (IllegalArgumentException | IllegalStateException invalid_req) {
            broadcastTelemetry("Mossa non valida: " + invalid_req.getMessage());
        } catch (IOException stream_exc) {
            broadcastTelemetry("Errore di connessione: " + stream_exc.getMessage());
        }
    }

    private void mutateCellState(int physical_index, boolean authorized_by_local) {
        Button block_trigger = topological_map.get(physical_index);
        if (block_trigger != null) {
            char active_glyph = authorized_by_local ? local_glyph : external_glyph;

            block_trigger.setText(String.valueOf(active_glyph));
            block_trigger.setDisable(true);

            if (active_glyph == 'X') {
                block_trigger.getStyleClass().add("x-symbol");
            } else if (active_glyph == 'O') {
                block_trigger.getStyleClass().add("o-symbol");
            }
        }
    }

    private void freezeTopologicalGrid() {
        topological_map.values().forEach(btn -> btn.setDisable(true));
    }

    private void initializeDaemonHook() {
        network_gateway.startReader(new GameClient.GameEvents() {
            @Override
            public void onFirstTurn(int token_holder) {
                boolean local_priority = (token_holder == network_gateway.getPlayerId());
                has_token_control = local_priority;

                local_glyph = local_priority ? 'X' : 'O';
                external_glyph = local_priority ? 'O' : 'X';

                simulation_engine = new TrisGame();
                simulation_engine.setMyMark(local_priority ? 1 : 2);

                Platform.runLater(() -> {
                    broadcastTelemetry(local_priority ? "È il tuo turno! (Il tuo simbolo: '" + local_glyph + "')"
                            : "Turno dell'avversario (Il tuo simbolo: '" + local_glyph + "')");

                    backButton.setVisible(false);
                });
            }

            @Override
            public void onOpponentMove(int external_delta) {
                Platform.runLater(() -> {
                    simulation_engine.applyOpponentMove(external_delta);
                    mutateCellState(external_delta, false);
                    has_token_control = true;
                    broadcastTelemetry("L'avversario ha mosso. È il tuo turno!");

                });
            }

            @Override
            public void onGameStatus(int session_state_flag) {
                Platform.runLater(() -> {
                    if (session_state_flag == -2)
                        return;
                    if (session_state_flag == -1) {
                        broadcastTelemetry("Pareggio! Nessuno ha vinto.");
                    } else {
                        if (session_state_flag == network_gateway.getPlayerId()) {
                            broadcastTelemetry("Hai vinto la partita!");
                        } else {
                            broadcastTelemetry("Hai perso la partita.");
                        }
                    }
                    freezeTopologicalGrid();
                    revealFallbackTrigger();
                    /* P3: Rivincita proposta per tutti gli esiti */
                    promptForSessionRefresh();
                });
            }

            @Override
            public void onConnectionError(IOException e) {
                Platform.runLater(() -> broadcastTelemetry("Connessione interrotta: " + e.getMessage()));
            }
        });
    }

    private void broadcastTelemetry(String payload_message) {
        if (statusLabel != null)
            statusLabel.setText(payload_message);
        System.out.println(payload_message);
    }

}
