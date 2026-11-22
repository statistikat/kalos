#!/bin/sh
# 21-I live probe runner: compile I21Probe.java against the pinned 3.9.0 stack
# (sa/toolkit + x13 + tramoseats jars) and run one mode. Usage: ./run21.sh <tag> <mode> [args...]
# Same JVM + G2 intrinsic-disable recipe as m5probe/run.sh (pinned OpenJDK 21).
set -e
cd "$(dirname "$0")"
JARS=/data/home/kowa/R/x86_64-pc-linux-gnu-library/4.5/rjd3jars/java
TK=/data/home/kowa/R/x86_64-pc-linux-gnu-library/4.5/rjd3toolkit/java
TS=/data/home/kowa/R/x86_64-pc-linux-gnu-library/4.5/rjd3tramoseats/java
X13=/data/home/kowa/R/x86_64-pc-linux-gnu-library/4.5/rjd3x13/java
CP="$JARS/jdplus-sa-base-api-3.9.0.jar:$JARS/jdplus-sa-base-core-3.9.0.jar:$JARS/jdplus-toolkit-base-core-3.9.0.jar:$JARS/jdplus-toolkit-base-api-3.9.0.jar:$JARS/protobuf-java-4.36.1.jar:$TK/jdplus-toolkit-base-protobuf-3.9.0.jar:$TK/jdplus-sa-base-r-3.9.0.jar:$TK/jdplus-toolkit-base-r-3.9.0.jar:$TK/jdplus-sa-base-protobuf-3.9.0.jar:$X13/jdplus-x13-base-protobuf-3.9.0.jar:$X13/jdplus-x13-base-api-3.9.0.jar:$X13/jdplus-x13-base-r-3.9.0.jar:$X13/jdplus-x13-base-core-3.9.0.jar:$TS/jdplus-tramoseats-base-protobuf-3.9.0.jar:$TS/jdplus-tramoseats-base-api-3.9.0.jar:$TS/jdplus-tramoseats-base-r-3.9.0.jar:$TS/jdplus-tramoseats-base-core-3.9.0.jar"
[ -d classes ] || mkdir -p classes
/usr/bin/javac --release 21 -cp "$CP" -d classes I21Probe.java
tag=$1; shift
mkdir -p "out/$tag"
name="$1-$2-$3"
name=$(echo "$name" | sed 's/-$//')
env _JAVA_OPTIONS="-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp -XX:DisableIntrinsic=_dlog" \
  /usr/lib/jvm/java-21-openjdk-amd64/bin/java -cp "classes:$CP" I21Probe "$@" > "out/$tag/$name.out" 2> "out/$tag/$name.err"
echo "wrote out/$tag/$name.out ($(wc -l < "out/$tag/$name.out") lines)"
