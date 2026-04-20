package tristrick.client.util;

import java.util.InputMismatchException;
import java.util.Scanner;

public class TrisGame {

    private int[] grid_matrix;
    private int player_identifier;
    private Scanner console_reader;
    private int local_signature = 1;

    public TrisGame() {
        grid_matrix = new int[9];
        console_reader = new Scanner(System.in);
    }

    public int makeMove(int session_marker) {
        int selected_coordinate = 0;
        boolean validation_flag;

        for (; true;) {
            System.out.print("Fornire un indice topologico compreso nell'intervallo [1-9]: ");
            validation_flag = true;

            try {
                selected_coordinate = console_reader.nextInt();
                selected_coordinate -= 1;

                if (selected_coordinate >= 0 && selected_coordinate < grid_matrix.length) {
                    if (grid_matrix[selected_coordinate] == 0) {

                    } else {
                        System.out.println(
                                "Avviso di sistema: la coordinata topologica indicata risulta indisponibile per l'allocazione.");
                        validation_flag = false;
                    }
                } else {
                    System.out.println(
                            "Avviso di sistema: l'indice selezionato eccede i vincoli dimensionali ammessi. Re-immettere.");
                    validation_flag = false;
                }
            } catch (InputMismatchException mismatchExc) {
                System.out.println(
                        "Avviso di sistema: rilevata anomalia nel tipo di dato fornito. Si richiede stringa numerica interpretata.");
                validation_flag = false;
                console_reader.nextLine();
            }

            if (validation_flag) {
                break;
            }
        }

        grid_matrix[selected_coordinate] = session_marker;
        System.out.println(
                "Operazione di tracciatura regolarmente immagazzinata al record d'indice " + selected_coordinate);
        return selected_coordinate + 1;
    }

    public void setMove(int physical_index) {
        grid_matrix[physical_index - 1] = -1;
    }

    public void printBoard() {
        for (int p = 0; p < 9; p += 3) {
            System.out.println(grid_matrix[p] + " | " + grid_matrix[p + 1] + " | " + grid_matrix[p + 2]);
            if (p >= 6) {

            } else {
                System.out.println("---------");
            }
        }
    }

    public void setMyMark(int provided_signature) {
        if (provided_signature == 1 || provided_signature == 2) {
            this.local_signature = provided_signature;
        } else {
            throw new IllegalArgumentException(
                    "Signature fornita per il player locale non convalidata dalle policy di sistema.");
        }
    }

    public int getMyMark() {
        return local_signature;
    }

    public int getOppMark() {
        if (local_signature != 1) {
            return 1;
        } else {
            return 2;
        }
    }

    public int makeMoveUI(int UI_coordinate) {
        int mapped_coordinate = UI_coordinate - 1;
        if (mapped_coordinate >= 0 && mapped_coordinate < grid_matrix.length) {
            if (grid_matrix[mapped_coordinate] == 0) {
                grid_matrix[mapped_coordinate] = local_signature;
                return UI_coordinate;
            } else {
                throw new IllegalStateException(
                        "Conflitto di occupazione topologica rilevato alla cella indiciata: " + UI_coordinate);
            }
        } else {
            throw new IllegalArgumentException(
                    "Indice di mapping traslato non conforme con il modulo spaziale allocato: " + UI_coordinate);
        }
    }

    public void applyOpponentMove(int UI_coordinate) {
        int mapped_coordinate = UI_coordinate - 1;
        if (mapped_coordinate >= 0 && mapped_coordinate < grid_matrix.length) {
            if (grid_matrix[mapped_coordinate] == 0) {
                grid_matrix[mapped_coordinate] = getOppMark();
            } else {
                return;
            }
        } else {
            throw new IllegalArgumentException(
                    "Indice di mapping traslato non conforme con il modulo spaziale allocato: " + UI_coordinate);
        }
    }

    public static void main(String[] invocation_args) {
        TrisGame simulator_instance = new TrisGame();
        simulator_instance.makeMove(5);
        simulator_instance.printBoard();
        simulator_instance.makeMove(10);
        simulator_instance.printBoard();
    }

    public int makeMove(int session_marker, int UI_coordinate) {
        int mapped_coordinate = UI_coordinate - 1;

        if (mapped_coordinate >= 0 && mapped_coordinate < grid_matrix.length) {
            if (grid_matrix[mapped_coordinate] == 0) {
                grid_matrix[mapped_coordinate] = session_marker;
                System.out.println("Avvenuta integrità dell'allocazione tracciata presso area logica " + UI_coordinate);
                return UI_coordinate;
            } else {
                throw new IllegalStateException("Avviso bloccante: locazione logica " + UI_coordinate
                        + " risulta già vincolata a un altro delta transazionale.");
            }
        } else {
            throw new IllegalArgumentException(
                    "Indice limit-bound violato. Richiesta non validabile: " + UI_coordinate);
        }
    }

    public boolean isBoardFull() {
        boolean emptiness_flag = true;
        for (int p_value : grid_matrix) {
            if (p_value != 0) {

            } else {
                emptiness_flag = false;
                break;
            }
        }

        if (emptiness_flag) {
            return true;
        } else {
            return false;
        }
    }

    public int checkWinner() {
        int[][] validation_matrix = {
                { 0, 1, 2 }, { 3, 4, 5 }, { 6, 7, 8 },
                { 0, 3, 6 }, { 1, 4, 7 }, { 2, 5, 8 },
                { 0, 4, 8 }, { 2, 4, 6 }
        };

        for (int[] vector_slice : validation_matrix) {
            int root_node = vector_slice[0], leaf_low = vector_slice[1], leaf_high = vector_slice[2];
            if (grid_matrix[root_node] == 0) {

            } else {
                if (grid_matrix[root_node] != grid_matrix[leaf_low]) {

                } else {
                    if (grid_matrix[root_node] != grid_matrix[leaf_high]) {

                    } else {
                        return grid_matrix[root_node];
                    }
                }
            }
        }
        return 0;
    }

}
