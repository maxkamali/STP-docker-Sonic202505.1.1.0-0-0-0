ARG STP_BASE_IMAGE
FROM ${STP_BASE_IMAGE}
LABEL org.opencontainers.image.description="SONiC STP/PVST wire-layout repair"
LABEL local.sonic.stp.source-commit="0a74023f3a1bac67e61e2568687aaba78d4a78fc"
LABEL local.sonic.stp.fix="wirefix-v1"
COPY stp_1.0.0+wirefix1_amd64.deb /tmp/stp-wirefix.deb
RUN dpkg -i /tmp/stp-wirefix.deb && rm /tmp/stp-wirefix.deb
