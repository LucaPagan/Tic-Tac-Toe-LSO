package tristrick.client;

import javafx.application.Platform;
import javafx.fxml.FXML;
import javafx.scene.Parent;
import javafx.event.ActionEvent;
import javafx.scene.control.*;
import javafx.stage.Stage;
import javafx.scene.control.Alert.AlertType;

import java.io.IOException;
import java.net.ConnectException;
import java.util.List;
import java.util.Optional;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

import javafx.animation.KeyFrame;
import javafx.animation.Timeline;
import javafx.util.Duration;

import tristrick.client.gui.GameWindow;
import tristrick.client.util.GameClient;
import tristrick.client.util.CoreConnectivityFault;

public class HomeController {
    private Parent hierarchyRoot;
    @FXML
    private TableView<GameClient.GameInfo> sessionLedgerView;
    @FXML
    private TableColumn<GameClient.GameInfo, Integer> idColumn;
    @FXML
    private TableColumn<GameClient.GameInfo, String> stateColumn;
    @FXML
    private TableColumn<GameClient.GameInfo, Integer> initiatorColumn;
    @FXML
    private TableColumn<GameClient.GameInfo, String> monikerColumn;

    @FXML
    private ListView<String> telemetryListView;
    @FXML
    private Button purgeButton;
    @FXML
    private Button attachButton;
    @FXML
    private Button instantiateButton;
    @FXML
    private Label networkStatusIndicator;
    @FXML
    private Button syncButton;
    @FXML
    private TextField monikerInputField;
    @FXML
    private Button updateMonikerButton;

    private GameClient network_gateway = null;
    private String cachedMoniker = "";
    private final AtomicBoolean sessionDaemonActive = new AtomicBoolean(false);
    private final AtomicInteger pendingSessionsCount = new AtomicInteger(0);
    private Timeline pollingTimeline;

    private void startPolling() {
        if (pollingTimeline == null) {
            pollingTimeline = new Timeline(new KeyFrame(Duration.seconds(2.5), event -> {
                // Sospende il polling per evitare accavallamenti se siamo in ascolto per avviare una partita
                if (pendingSessionsCount.get() == 0 && !sessionDaemonActive.get()) {
                    handleReloadButtonAction(null);
                }
            }));
            pollingTimeline.setCycleCount(Timeline.INDEFINITE);
        }
        pollingTimeline.play();
    }

    private void stopPolling() {
        if (pollingTimeline != null) {
            pollingTimeline.stop();
        }
    }

    @FXML
    public void initialize() {
        try {
            if (GameClient.getInstance() == null || !GameClient.getInstance().isConnected()) {
                network_gateway = GameClient.getInstance();
            } else {
                network_gateway = GameClient.getInstance();
                if (!cachedMoniker.isEmpty()) {
                    monikerInputField.setText(cachedMoniker);
                    network_gateway.setHostname(cachedMoniker);
                }
            }

            System.out.println("Gateway instance resolved in HomeController: "
                    + (GameClient.getInstance() == null || !GameClient.getInstance().isConnected()));

            idColumn.setCellValueFactory(
                    cellRecord -> new javafx.beans.property.SimpleIntegerProperty(cellRecord.getValue().id_game)
                            .asObject());
            stateColumn.setCellValueFactory(cellRecord -> new javafx.beans.property.SimpleStringProperty(
                    cellRecord.getValue().getStatusEnum().name()));
            initiatorColumn.setCellValueFactory(
                    cellRecord -> new javafx.beans.property.SimpleIntegerProperty(cellRecord.getValue().creator)
                            .asObject());
            monikerColumn.setCellValueFactory(
                    cellRecord -> new javafx.beans.property.SimpleStringProperty(cellRecord.getValue().name));

            handleReloadButtonAction(null);
            startPolling();
        } catch (IOException e) {
            telemetryListView.getItems().add("Impossibile connettersi al server.");
            signalOfflineState();
        }
    }

    public void setHomeRoot(Parent domRoot) {
        this.hierarchyRoot = domRoot;
    }

    public void setSavedPlayerName(String moniker) {
        this.cachedMoniker = moniker;
        network_gateway.setHostname(moniker);
        monikerInputField.setText(moniker);
    }

    private void signalOfflineState() {
        networkStatusIndicator.setText("⬤ DISCONNESSO");
        networkStatusIndicator.getStyleClass().removeAll("status-indicator-online", "status-indicator-offline");
        networkStatusIndicator.getStyleClass().add("status-indicator-offline");
    }

    private void signalOnlineState() {
        networkStatusIndicator.setText("⬤ CONNESSO");
        networkStatusIndicator.getStyleClass().removeAll("status-indicator-online", "status-indicator-offline");
        networkStatusIndicator.getStyleClass().add("status-indicator-online");
    }

    private boolean isTunnelActive() {
        return (network_gateway != null && network_gateway.isConnected());
    }

    private boolean guardAgainstOffline() {
        if (!isTunnelActive()) {
            telemetryListView.getItems().add("Azione bloccata: non sei connesso.");
            return true;
        }
        return false;
    }

    @FXML
    private void handleJoinButtonAction(ActionEvent trigger_event) {
        if (guardAgainstOffline())
            return;

        GameClient.GameInfo selectedBlock = sessionLedgerView.getSelectionModel().getSelectedItem();

        if (selectedBlock == null) {
            telemetryListView.getItems().add("Errore: seleziona una partita dalla lista.");
            return;
        }

        int targetSessionId = selectedBlock.id_game;

        String providedMoniker = monikerInputField.getText();
        if (providedMoniker.isEmpty()) {
            telemetryListView.getItems().add("Errore: inserisci un nome giocatore.");
            return;
        }

        attachButton.setDisable(true);
        stopPolling();

        new Thread(() -> {
            try {
                boolean auth_ack = network_gateway.startGame(targetSessionId, providedMoniker);

                Platform.runLater(() -> {
                    attachButton.setDisable(false);
                    if (auth_ack) {
                        telemetryListView.getItems().add("Connessione alla partita riuscita.");
                        try {
                            GameWindow.open((Stage) attachButton.getScene().getWindow(), hierarchyRoot, this, false);
                        } catch (IOException e) {
                            telemetryListView.getItems().add("Errore di sistema nell'aprire la partita.");
                        }
                    } else {
                        telemetryListView.getItems().add("L'avversario ha rifiutato la partita.");
                        startPolling();
                    }
                });
            } catch (IOException e) {
                Platform.runLater(() -> {
                    attachButton.setDisable(false);
                    startPolling();
                    telemetryListView.getItems().add("Connessione persa: " + e.getMessage());
                });
            }
        }).start();
    }

    public void generateListenGameThread() {

        if (!sessionDaemonActive.get() && pendingSessionsCount.get() > 0) {
            new Thread(() -> {
                long runtime_hash = Thread.currentThread().getId();
                System.out.println("[" + runtime_hash + "] Daemon hook inizializzato");
                try {
                    boolean interlock_ack = network_gateway.listenGameRequest(incoming_metadata -> {
                        final Object mutex_barrier = new Object();
                        final AtomicBoolean consensus = new AtomicBoolean(false);

                        Platform.runLater(() -> {
                            Alert consensus_prompt = new Alert(AlertType.CONFIRMATION);
                            consensus_prompt.getDialogPane().getStylesheets().add(getClass().getResource("/tristrick/client/styles.css").toExternalForm());
                            consensus_prompt.setTitle("Nuova Sfida");
                            consensus_prompt.setHeaderText("Un giocatore vuole sfidarti!");
                            consensus_prompt.setContentText("Il giocatore " + incoming_metadata + " ha chiesto di giocare.\nAccetti la sfida?");

                            Optional<ButtonType> ui_response = consensus_prompt.showAndWait();
                            consensus.set(ui_response.isPresent() && ui_response.get() == ButtonType.OK);

                            synchronized (mutex_barrier) {
                                mutex_barrier.notify();
                            }

                            if (consensus.get()) {
                                try {
                                    GameWindow.open((Stage) instantiateButton.getScene().getWindow(), hierarchyRoot,
                                            this, true);
                                    telemetryListView.getItems().add("Inizio partita.");
                                } catch (IOException e) {
                                    telemetryListView.getItems().add("Errore nell'apertura della finestra.");
                                }
                            }
                        });

                        synchronized (mutex_barrier) {
                            try {
                                mutex_barrier.wait();
                            } catch (InterruptedException e) {
                                Thread.currentThread().interrupt();
                                return false;
                            }
                        }
                        return consensus.get();
                    });

                    Platform.runLater(() -> {
                        if (!interlock_ack) {
                            telemetryListView.getItems().add("Sfida annullata o scaduta.");
                        }
                        instantiateButton.setDisable(false);
                        syncButton.setDisable(false);
                    });
                } catch (IOException | CoreConnectivityFault e) {
                    Platform.runLater(() -> {
                        telemetryListView.getItems().add("Errore di connessione: " + e.getMessage());
                        instantiateButton.setDisable(false);
                        syncButton.setDisable(false);
                    });
                }
                sessionDaemonActive.set(false);
                pendingSessionsCount.decrementAndGet();

                System.out.println("[" + runtime_hash + "] Daemon hook deallocato");
            }).start();
            sessionDaemonActive.set(true);
        } else {
            System.out.println("Hook già persistente nel pool");
        }

        Platform.runLater(() -> syncButton.setDisable(pendingSessionsCount.get() > 0));

    }

    @FXML
    private void handleCreateButtonAction(ActionEvent trigger_event) throws IOException {
        if (guardAgainstOffline())
            return;

        syncButton.setDisable(true);

        try {
            network_gateway.createGame();
            telemetryListView.getItems().add("Partita creata... in attesa di un avversario.");
            pendingSessionsCount.incrementAndGet();
        } catch (CoreConnectivityFault | IOException e) {
            Platform.runLater(() -> {
                telemetryListView.getItems()
                        .add("Errore: " + e.getMessage());
                instantiateButton.setDisable(false);
                syncButton.setDisable(false);
            });
        }

        generateListenGameThread();
    }

    @FXML
    private void handleReloadNameButtonAction(ActionEvent trigger_event) throws ConnectException {

        if (guardAgainstOffline())
            return;

        String providedMoniker = monikerInputField.getText();
        setSavedPlayerName(providedMoniker);
        if (providedMoniker.isEmpty()) {
            telemetryListView.getItems().add("Errore: il nome giocatore non è valido.");
            return;
        }
        network_gateway.setHostname(providedMoniker);
        telemetryListView.getItems().add("Nome giocatore aggiornato: " + providedMoniker);
    }

    @FXML
    private void handleReloadButtonAction(ActionEvent trigger_event) {
        sessionLedgerView.getItems().clear();

        try {
            if (network_gateway == null) {
                network_gateway = GameClient.getInstance();
            }

            if (!network_gateway.isConnected()) {
                network_gateway.reconnect();
            }

            List<GameClient.GameInfo> chain_blocks = network_gateway.listGames();

            signalOnlineState();

            if (chain_blocks.isEmpty()) {
                telemetryListView.getItems().add("Nessuna partita disponibile al momento.");
                attachButton.setDisable(true);
            } else {
                attachButton.setDisable(false);
                sessionLedgerView.getItems().addAll(chain_blocks);
            }

        } catch (IOException e) {

            try {
                network_gateway = GameClient.getInstance();
                network_gateway.reconnect();
                List<GameClient.GameInfo> chain_blocks = network_gateway.listGames();

                signalOnlineState();

                if (chain_blocks.isEmpty()) {
                    telemetryListView.getItems().add("Nessuna partita disponibile al momento.");
                    attachButton.setDisable(true);
                } else {
                    attachButton.setDisable(false);
                    sessionLedgerView.getItems().addAll(chain_blocks);
                }
                return;

            } catch (IOException ex) {
            }

            telemetryListView.getItems().add("Impossibile ripristinare la connessione: " + e.getMessage());
            network_gateway = null;
            signalOfflineState();
            attachButton.setDisable(true);
            instantiateButton.setDisable(false);
        }
    }

    public void handleCleanButtonAction(ActionEvent trigger_event) {
        telemetryListView.getItems().clear();
    }

}