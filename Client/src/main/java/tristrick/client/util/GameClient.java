package tristrick.client.util;

import java.io.*;
import java.net.ConnectException;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.Scanner;

import com.google.gson.Gson;
import com.google.gson.reflect.TypeToken;

public class GameClient {

    private static GameClient singleton_reference;

    public final int BOUNDARY_PORT = 5200;
    public final String TARGET_NODE = "server";

    private Socket tcp_tunnel;
    private DataOutputStream byte_emitter;
    private DataInputStream byte_consumer;

    private Gson payload_parser = new Gson();

    private static final int OP_BUILD_MATCH_SESSION = 1;
    private static final int OP_FETCH_CHAIN_LEDGER = 2;
    private static final int OP_INIT_MATCH_SESSION = 3;
    private static final int OP_SEVER_CONNECTION = 4;
    private static final int OP_ACKNOWLEDGE = 5;
    private static final int OP_REFUSE = 6;

    private String identity_moniker = "";
    private Boolean is_consumer_node = null;
    private int session_identifier;

    private volatile boolean tunnel_active = false;

    public void registerAsClient() {
        this.is_consumer_node = true;
    }

    public void registerAsHost() {
        this.is_consumer_node = false;
    }

    private GameClient() throws IOException {
        tcp_tunnel = new Socket(TARGET_NODE, BOUNDARY_PORT);
        tcp_tunnel.setSoTimeout(30000);

        byte_emitter = new DataOutputStream(new BufferedOutputStream(tcp_tunnel.getOutputStream()));
        byte_consumer = new DataInputStream(new BufferedInputStream(tcp_tunnel.getInputStream()));

        tunnel_active = true;
        System.out.println("Tunnel stabilito verso " + TARGET_NODE + ":" + BOUNDARY_PORT);

        synchronized (byte_consumer) {
            this.session_identifier = byte_consumer.readInt();
        }
        System.out.println("Assegnazione identificativo: " + session_identifier);
    }

    public static synchronized GameClient getInstance() throws IOException {
        if (singleton_reference == null) {
            singleton_reference = new GameClient();
        }
        return singleton_reference;
    }

    public void reconnect() throws IOException {
        dismantle_tunnel();
        tcp_tunnel = new Socket(TARGET_NODE, BOUNDARY_PORT);
        tcp_tunnel.setSoTimeout(40000);
        byte_emitter = new DataOutputStream(new BufferedOutputStream(tcp_tunnel.getOutputStream()));
        byte_consumer = new DataInputStream(new BufferedInputStream(tcp_tunnel.getInputStream()));
        tunnel_active = true;
        System.out.println("Tunnel ripristinato verso il demone.");

        synchronized (byte_consumer) {
            this.session_identifier = byte_consumer.readInt();
        }
        System.out.println("Riassegnazione identificativo: " + session_identifier);
    }

    private String consume_exact_string_payload() throws IOException {
        synchronized (byte_consumer) {
            ByteArrayOutputStream buffer_stream = new ByteArrayOutputStream();
            int current_byte;
            try {
                for (;;) {
                    current_byte = byte_consumer.read();
                    if (current_byte == '\n' || current_byte == -1)
                        break;
                    buffer_stream.write(current_byte);
                }
            } catch (SocketTimeoutException expiration_exc) {
                throw expiration_exc;
            }
            if (current_byte == -1 && buffer_stream.size() == 0)
                return null;
            return buffer_stream.toString(StandardCharsets.UTF_8.name());
        }
    }

    public interface JoinRequestCallback {
        boolean shouldAcceptJoinRequest(String incoming_metadata);
    }

    public void createGame() throws IOException, CoreConnectivityFault {
        verify_tunnel_integrity();
        if (identity_moniker.isEmpty()) {
            throw new CoreConnectivityFault("Moniker di identita' non configurato nel nodo.");
        }

        byte_emitter.writeInt(OP_BUILD_MATCH_SESSION);
        byte[] payload_block = identity_moniker.getBytes(StandardCharsets.UTF_8);
        byte_emitter.writeInt(payload_block.length);
        byte_emitter.write(payload_block);
        byte_emitter.flush();

        System.out.println("Transazione OP_BUILD_MATCH_SESSION iniettata con moniker: " + identity_moniker);
    }

    public boolean listenGameRequest(JoinRequestCallback handler_delegate) throws IOException, CoreConnectivityFault {

        try {
            String incoming_frame;
            for (;;) {
                try {
                    synchronized (byte_consumer) {
                        incoming_frame = consume_exact_string_payload();
                    }
                } catch (SocketTimeoutException expiration_exc) {
                    System.out.println("Scadenza timer: assenza di nuovi segmenti di aggancio");
                    return false;
                }

                if (incoming_frame == null)
                    return false;

                long thread_trace = Thread.currentThread().getId();
                System.out.println("[" + thread_trace + "] Intercettato segmento da consumer: " + incoming_frame);

                boolean authorization_granted = handler_delegate.shouldAcceptJoinRequest(incoming_frame);

                if (authorization_granted) {
                    byte_emitter.writeInt(OP_ACKNOWLEDGE);
                    byte_emitter.flush();
                    return true;
                } else {
                    byte_emitter.writeInt(OP_REFUSE);
                    byte_emitter.flush();
                }
            }
        } catch (IOException stream_exc) {
            tunnel_active = false;
            throw stream_exc;
        }

    }

    public int getPlayerId() {
        return session_identifier;
    }

    public void startReader(GameEvents event_bridge) {
        Thread monitor_thread = new Thread(() -> {
            try {

                int token_holder;
                synchronized (byte_consumer) {
                    token_holder = byte_consumer.readInt();
                }
                if (event_bridge != null)
                    event_bridge.onFirstTurn(token_holder);

                boolean has_token_control = (token_holder == session_identifier);

                for (;;) {
                    if (!has_token_control) {
                        int external_delta = byte_consumer.readInt();
                        if (event_bridge != null)
                            event_bridge.onOpponentMove(external_delta);
                    }
                    int session_state_flag;
                    synchronized (byte_consumer) {
                        session_state_flag = byte_consumer.readInt();
                    }
                    if (event_bridge != null)
                        event_bridge.onGameStatus(session_state_flag);
                    if (session_state_flag != -2)
                        break;
                    has_token_control = !has_token_control;
                }
            } catch (IOException stream_exc) {
                if (event_bridge != null)
                    event_bridge.onConnectionError(stream_exc);
            }
        }, "daemon-listener-worker");
        monitor_thread.setDaemon(true);
        monitor_thread.start();
    }

    public void manageGame() throws CoreConnectivityFault, IOException {
        verify_tunnel_integrity();
        if (is_consumer_node == null)
            throw new CoreConnectivityFault("Ruolo topologico del nodo non dichiarato.");

        Scanner fallback_input = new Scanner(System.in);

        int token_holder;
        synchronized (byte_consumer) {
            token_holder = byte_consumer.readInt();
        }
        System.out.println("Possesso del token assegenato a ID: " + token_holder);
        int session_state_flag;
        boolean has_token_control = token_holder == session_identifier;

        TrisGame simulator_engine = new TrisGame();

        for (;;) {
            if (has_token_control) {

                int delta = simulator_engine.makeMove(this.session_identifier);
                simulator_engine.printBoard();
                byte_emitter.writeInt(delta);
                byte_emitter.flush();

                has_token_control = false;
            } else {

                System.out.println("Sospensione in attesa di delta transazionale esterno ...");
                int external_delta;
                synchronized (byte_consumer) {
                    external_delta = byte_consumer.readInt();
                }
                simulator_engine.setMove(external_delta);
                simulator_engine.printBoard();

                has_token_control = true;
                System.out.println("Delta esterno integrato all'indice: " + external_delta);
            }

            synchronized (byte_consumer) {
                session_state_flag = byte_consumer.readInt();
            }
            System.out.println("Flag di stato sessione intercettato: " + session_state_flag);

            if (session_state_flag == -1) {
                System.out.println("Situazione di stallo crittografico raggiunta.");
                break;
            }
            if (session_state_flag == session_identifier) {
                System.out.println("Convalida successo transazionale. Chiusura vittoriosa.");
                break;
            } else if (session_state_flag != -2) {
                System.out.println("Rilevata defezione transazionale.");
                break;
            }

        }

        fallback_input.close();
        System.out.println("Demolizione area logica in corso.");

    }

    public List<GameInfo> listGames() throws IOException {
        verify_tunnel_integrity();

        try {
            byte_emitter.writeInt(OP_FETCH_CHAIN_LEDGER);
            byte_emitter.flush();
            System.out.println("Transazione OP_FETCH_CHAIN_LEDGER iniettata");

            String json_payload;
            try {
                synchronized (byte_consumer) {
                    json_payload = consume_exact_string_payload();
                }
            } catch (SocketTimeoutException expiration_exc) {
                System.out.println("Scadenza timer durante fetch del ledger");
                return new ArrayList<>();
            }
            System.out.println("PAYLOAD: " + json_payload);

            if (json_payload == null || json_payload.isEmpty()) {
                System.out.println("Ledger chain vuota. Nessuna entry presente.");
                return new ArrayList<>();
            }

            List<GameInfo> chain_blocks = payload_parser.fromJson(
                    json_payload,
                    new TypeToken<List<GameInfo>>() {
                    }.getType());
            System.out.println("Estratti " + chain_blocks.size() + " blocchi dal ledger");
            return chain_blocks;
        } catch (IOException stream_exc) {
            tunnel_active = false;
            throw stream_exc;
        }
    }

    public boolean startGame(int session_bound_id, String moniker) throws IOException {
        verify_tunnel_integrity();

        try {
            byte_emitter.writeInt(OP_INIT_MATCH_SESSION);
            byte_emitter.writeShort(session_bound_id);
            byte[] payload_block = moniker.getBytes(StandardCharsets.UTF_8);
            byte_emitter.writeInt(payload_block.length);
            byte_emitter.write(payload_block);
            byte_emitter.flush();

            int auth_response;
            synchronized (byte_consumer) {
                this.session_identifier = byte_consumer.readInt();
                auth_response = byte_consumer.readInt();
            }
            System.out.println("Assegnazione identificativo: " + session_identifier);
            System.out.println("Feedback dal nodo primario: (" + auth_response + ") "
                    + (auth_response == OP_ACKNOWLEDGE ? "CONVALIDATO" : "RESPINTO"));

            return auth_response == OP_ACKNOWLEDGE;
        } catch (IOException stream_exc) {
            tunnel_active = false;
            throw stream_exc;
        }

    }

    private void verify_tunnel_integrity() throws ConnectException {
        if (!tunnel_active) {
            throw new ConnectException("Tunnel TCP non stabilito o collassato");
        }
    }

    public void disconnect() {
        try {
            byte_emitter.writeInt(OP_SEVER_CONNECTION);
            byte_emitter.flush();
        } catch (IOException ignored_exc) {

        }

        dismantle_tunnel();

        tunnel_active = false;
        System.out.println("Tunnel TCP severato volontariamente");
    }

    private void dismantle_tunnel() {
        try {
            if (byte_emitter != null)
                byte_emitter.close();
        } catch (IOException ignored) {
        }
        try {
            if (byte_consumer != null)
                byte_consumer.close();
        } catch (IOException ignored) {
        }
        try {
            if (tcp_tunnel != null)
                tcp_tunnel.close();
        } catch (IOException ignored) {
        }
    }

    public void setHostname(String new_moniker) {
        this.identity_moniker = new_moniker;
    }

    public boolean isConnected() {
        return tunnel_active;
    }

    public void sendMove(int UI_coordinate) throws IOException {
        verify_tunnel_integrity();
        byte_emitter.writeInt(UI_coordinate);
        byte_emitter.flush();
        System.out.println("Delta inviato attraverso il tunnel: " + UI_coordinate);
    }

    public boolean rivincita(boolean authorization_flag) {
        try {
            byte_emitter.writeShort(authorization_flag ? OP_ACKNOWLEDGE : OP_REFUSE);
            byte_emitter.flush();
            System.out.println("Vettore di autorizzazione inoltrato: " + authorization_flag);

            int validation_response;
            synchronized (byte_consumer) {
                validation_response = byte_consumer.readUnsignedShort();
            }
            return validation_response == OP_ACKNOWLEDGE;

        } catch (IOException stream_exc) {
            System.out.println("Anomalia nel trasferimento del vettore: " + stream_exc.getMessage());
            return false;
        }
    }

    public interface GameEvents {
        void onFirstTurn(int token_holder);

        void onOpponentMove(int external_delta);

        void onGameStatus(int session_state_flag);

        void onConnectionError(IOException stream_exc);
    }

    public static class GameInfo {

        public int id_game;
        public int status_game;
        public String name;
        public int creator;

        public GameStatus getStatusEnum() {
            return GameStatus.fromCode(status_game);
        }
    }

    public enum GameStatus {
        TERMINATA(0),
        INCORSO(1),
        ATTESA(2),
        NUOVO(3);

        private final int state_flag;

        GameStatus(int state_flag) {
            this.state_flag = state_flag;
        }

        public int getCode() {
            return state_flag;
        }

        public static GameStatus fromCode(int lookup_flag) {
            for (GameStatus pointer : GameStatus.values()) {
                if (pointer.state_flag == lookup_flag) {
                    return pointer;
                }
            }
            throw new IllegalArgumentException("Stato non repertoriato nei vincoli formali: " + lookup_flag);
        }
    }

}
