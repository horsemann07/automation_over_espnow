# Automation over ESP-NOW

This project focuses on implementing automation capabilities using ESP-NOW communication protocol, taking inspiration from the concepts of alpha, beta, and gamma from the [VSCP (Very Simple Control Protocol)](https://grodansparadis.github.io/vscp-doc-spec/#/) framework. It leverages the VSCP message format, class, and type to enable seamless communication and coordination between different types of nodes for automation tasks.

## Description

The Automation over ESP-NOW project utilizes the concept of alpha, beta, and gamma nodes to create an automation network. It should be noted that while we draw inspiration from the VSCP framework, we are not implementing the entire VSCP protocol stack. Instead, we are focusing on specific concepts and elements:

### Note

Please note that this project is still under development and is not considered a final release. It is a work in progress, and certain features or functionalities may not be fully implemented or may be subject to changes.

### Alpha Concept
* Alpha nodes are always powered devices connected to a Wi-Fi router.
* Alpha nodes can be connected together using either Wi-Fi or ESP-NOW.
* Provisioning of alpha nodes is done using BLE (Bluetooth Low Energy) or SoftAP.
* Alpha nodes support over-the-air (OTA) firmware updates for themselves and remote beta and gamma nodes.
* It can also provide technical and status information about the cluster devices.
* Alpha nodes can connect with an MQTT hub to send/receive VSCP events, solved cloud queries.
* Alpha nodes act as time servers for the segment they are on, keeping clocks updated on all nodes in the cluster.
* Alpha nodes events can be configured and controlled using VSCP events.
* They have an initialization button with different press actions:
 TODO:
  - [ ] Short press: Enables secure pairing with other nodes, transferring encryption key and communication channel.
  - [ ] Long press (more than 3 seconds): Resets the node to factory defaults, requiring new provisioning.
  - [ ] Double-click: Starts Wi-Fi provisioning using BLE or SoftAP.
* Alpha nodes have a status LED that provides information about the device's status through different blinking patterns.
TODO: Blinking pattern.

### Beta Concept

* Beta nodes are powered/battery devices that communicate using VSCP over ESP-NOW.
* They connect to alpha nodes, other beta nodes, and gamma nodes within a communication cluster.
* Beta nodes events can be configured and controlled using VSCP events.
* Beta nodes can act as relay nodes to extend the range.
* They have an initialization button with different press actions similar to alpha nodes.

### Gamma Concept

* Gamma nodes are battery-powered devices that sleep most of the time.
* They communicate using VSCP over ESP-NOW and connect to alpha nodes, beta nodes, and other gamma nodes.
* Gamma nodes wake up for short periods and send events, receive data, and act on pending information.
* Alpha nodes can be used as a postbox to hold events for gamma nodes when they wake up over MQTT broker.
* Gamma nodes can be configured and controlled using VSCP events.
* Gamma nodes have an initialization button with different press actions similar to alpha nodes.

## Getting Started

- [ ] TODO

## Contributing

- [ ] TODO

## License
The Automation over ESP-NOW project is licensed under the MIT License. You are free to modify and distribute the codebase following the terms of the license.

## Acknowledgements
We extend our gratitude to the VSCP community for their valuable insights and the ESP-NOW development team for providing a reliable communication protocol for automation purposes.

