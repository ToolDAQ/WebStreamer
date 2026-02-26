#! /bin/bash

# nodedaemons
for i in {1..100}
do
    echo "{\"msg_id\":8, \"msg_time\":\"2026-02-26T04:47:34.657365Z\", \"msg_type\":\"Service Discovery\", \"msg_value\":\" Node Daemon$i\", \"remote_port\":24000, \"status\":\"Online\", \"status_query\":1, \"uuid\":\"e155691f-ed59-4bbf-8a68-c52a32c3fb9d\"}"
done

# RBUs
for i in {1..56}
do
    echo "{\"msg_id\":8, \"msg_time\":\"2026-02-26T04:47:34.657365Z\", \"msg_type\":\"Service Discovery\", \"msg_value\":\" RBU$i\", \"remote_port\":24000, \"status\":\"Online\", \"status_query\":1, \"uuid\":\"e155691f-ed59-4bbf-8a68-c52a32c3fb9d\"}"
done

# TPUs
for i in {1..30}
do
    echo "{\"msg_id\":8, \"msg_time\":\"2026-02-26T04:47:34.657365Z\", \"msg_type\":\"Service Discovery\", \"msg_value\":\" TPU$i\", \"remote_port\":24000, \"status\":\"Online\", \"status_query\":1, \"uuid\":\"e155691f-ed59-4bbf-8a68-c52a32c3fb9d\"}"
done

# EBU
for i in {1..5}
do
    echo "{\"msg_id\":8, \"msg_time\":\"2026-02-26T04:47:34.657365Z\", \"msg_type\":\"Service Discovery\", \"msg_value\":\" EBU$i\", \"remote_port\":24000, \"status\":\"Online\", \"status_query\":1, \"uuid\":\"e155691f-ed59-4bbf-8a68-c52a32c3fb9d\"}"
done

# Broker
for i in {1..2}
do
    echo "{\"msg_id\":8, \"msg_time\":\"2026-02-26T04:47:34.657365Z\", \"msg_type\":\"Service Discovery\", \"msg_value\":\" Broker$i\", \"remote_port\":24000, \"status\":\"Online\", \"status_query\":1, \"uuid\":\"e155691f-ed59-4bbf-8a68-c52a32c3fb9d\"}"
done


# Services
for i in {1..4}
do
    echo "{\"msg_id\":8, \"msg_time\":\"2026-02-26T04:47:34.657365Z\", \"msg_type\":\"Service Discovery\", \"msg_value\":\" Services$i\", \"remote_port\":24000, \"status\":\"Online\", \"status_query\":1, \"uuid\":\"e155691f-ed59-4bbf-8a68-c52a32c3fb9d\"}"
done


# DPB
for i in {1..1000}
do
    echo "{\"msg_id\":8, \"msg_time\":\"2026-02-26T04:47:34.657365Z\", \"msg_type\":\"Service Discovery\", \"msg_value\":\" DPB$i\", \"remote_port\":24000, \"status\":\"Online\", \"status_query\":1, \"uuid\":\"e155691f-ed59-4bbf-8a68-c52a32c3fb9d\"}"
done

# MPMT
for i in {1..808}
do
    echo "{\"msg_id\":8, \"msg_time\":\"2026-02-26T04:47:34.657365Z\", \"msg_type\":\"Service Discovery\", \"msg_value\":\" MPMT$i\", \"remote_port\":24000, \"status\":\"Online\", \"status_query\":1, \"uuid\":\"e155691f-ed59-4bbf-8a68-c52a32c3fb9d\"}"
done

