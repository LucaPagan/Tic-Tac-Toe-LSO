package tristrick.client.util;

public class CoreConnectivityFault extends Exception {

    public CoreConnectivityFault() {
        super();
    }

    public CoreConnectivityFault(String payloadMessage) {
        super(payloadMessage);
    }

}
