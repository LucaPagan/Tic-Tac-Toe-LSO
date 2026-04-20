# ===== Fase 1: Build con Maven =====
FROM maven:3.9.6-eclipse-temurin-22 AS builder

WORKDIR /build

# Copia tutto il progetto client
COPY . .

# Compila il JAR shaded (con dipendenze e manifest)
RUN mvn clean package -DskipTests

# ===== Fase 2: Runtime con Java + JavaFX =====
FROM eclipse-temurin:22-jdk

# Installa librerie grafiche native richieste da JavaFX
RUN apt-get update && apt-get install -y \
    libgtk-3-0 \
    libx11-xcb1 \
    libxtst6 \
    libxrandr2 \
    libfreetype6 \
    libnss3 \
    libxss1 \
    libxrender1 \
    libxext6 \
    libxi6 \
    libpulse0 \
    libglib2.0-0 \
    libgl1 \
    libgl1-mesa-dri \
    libgbm1 \
    libcanberra-gtk3-module \
    libcanberra-gtk-module \
    x11-apps \
    unzip curl

WORKDIR /app

# Scarica JavaFX SDK (rileva automaticamente x64 o aarch64 per Mac M1/M2/M3)
ENV JAVAFX_VERSION=24.0.2
RUN apt-get update && apt-get install -y unzip curl && \
    ARCH=$(uname -m) && \
    if [ "$ARCH" = "x86_64" ]; then JFX_ARCH="x64"; else JFX_ARCH="aarch64"; fi && \
    curl -L -o javafx.zip https://download2.gluonhq.com/openjfx/${JAVAFX_VERSION}/openjfx-${JAVAFX_VERSION}_linux-${JFX_ARCH}_bin-sdk.zip && \
    unzip javafx.zip && mv javafx-sdk-${JAVAFX_VERSION}/lib javafx-lib && rm -rf javafx.zip javafx-sdk-${JAVAFX_VERSION}

# Copia il JAR dalla fase builder
COPY --from=builder /build/target/client-1.0-SNAPSHOT-shaded.jar ./app.jar

ENV LD_LIBRARY_PATH=/app/javafx-lib
ENV JAVA_TOOL_OPTIONS="-Dprism.order=sw -Dprism.verbose=true -Djava.library.path=/app/javafx-lib"
CMD echo "DISPLAY is set to $DISPLAY" && java --module-path javafx-lib --add-modules javafx.controls,javafx.fxml -jar app.jar

# Avvio dell'app JavaFX
# CMD ["java", "--module-path", "javafx-lib", "--add-modules", "javafx.controls,javafx.fxml", "-jar", "app.jar"]
