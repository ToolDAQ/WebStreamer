#! /bin/bash 
echo {[
# nodedaemons
for i in {1..100}
do
    echo "{ \"msg_time\":\"2026-02-26T04:47:34.657365Z\", \"msg_value\":\" Node Daemon$i\", \"status\":\"Online\"},"
done

# RBUs
for i in {1..56}
do
    echo "{ \"msg_time\":\"2026-02-26T04:47:34.657365Z\", \"msg_value\":\" RBU$i\", \"status\":\"Online\"},"
done

# TPUs
for i in {1..30}
do
    echo "{ \"msg_time\":\"2026-02-26T04:47:34.657365Z\", \"msg_value\":\" TPU$i\", \"status\":\"Online\"},"
done

# EBU
for i in {1..5}
do
    echo "{ \"msg_time\":\"2026-02-26T04:47:34.657365Z\", \"msg_value\":\" EBU$i\", \"status\":\"Online\"},"
done

# Broker
for i in {1..2}
do
    echo "{ \"msg_time\":\"2026-02-26T04:47:34.657365Z\", \"msg_value\":\" Broker$i\", \"status\":\"Online\"},"
done


# Services
for i in {1..4}
do
    echo "{ \"msg_time\":\"2026-02-26T04:47:34.657365Z\", \"msg_value\":\" Services$i\", \"status\":\"Online\"},"
done


# DPB
for i in {1..1000}
do
    echo "{ \"msg_time\":\"2026-02-26T04:47:34.657365Z\", \"msg_value\":\" DPB$i\", \"status\":\"Online\"},"
done

# MPMT
for i in {1..808}
do
    echo "{ \"msg_time\":\"2026-02-26T04:47:34.657365Z\", \"msg_value\":\" MPMT$i\", \"status\":\"Online\"},"
done

echo ]}
