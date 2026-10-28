

🚨 Smart Evacuation Manager

📌 Project Overview

The Smart Evacuation Manager is a smart safety system designed to help people evacuate buildings during emergencies such as fires or hazardous air conditions.

The system uses Arduino, temperature sensors, and an air-quality sensor to monitor environmental conditions in different rooms. It identifies potentially dangerous rooms and uses Dijkstra's shortest-path algorithm to determine a suitable evacuation route.

The primary objective is to improve evacuation efficiency by guiding people toward safer exits while considering room-to-room connectivity and potential hazards.

🎯 Objectives

Monitor temperature and air quality in different rooms.

Identify rooms that may be dangerous during an emergency.

Represent a building as a graph of interconnected rooms.

Use Dijkstra's algorithm to calculate the shortest evacuation path.

Help people identify a suitable route toward an exit.

Reduce evacuation time and improve emergency response.

⚙️ Technologies Used

Arduino — Microcontroller for sensor integration and system control.

Temperature Sensor — Monitors room temperature.

Air-Quality Sensor — Detects changes in air quality that may indicate hazardous conditions.

Dijkstra's Algorithm — Finds the shortest path through the building's room network.

Graph Theory — Represents rooms as vertices and connecting passages as edges.

🏗️ System Working

Environmental Monitoring: Sensors collect temperature and air-quality readings from different rooms.

Risk Assessment: Sensor readings are evaluated against configured thresholds to identify potentially hazardous rooms.

Building Representation: Rooms are represented as vertices in a graph, with passages between rooms represented as edges.

Path Calculation: Dijkstra's algorithm calculates a shortest route from the current location to an exit.

Evacuation Guidance: The system identifies a suitable evacuation route based on the building layout and available risk information.

Note: The actual route-selection logic must account for hazardous rooms to ensure that a mathematically shortest route is not selected when it passes through a dangerous area.

🧠 Algorithm Used: Dijkstra's Shortest-Path Algorithm

Dijkstra's algorithm finds the shortest path between a starting vertex and other vertices in a weighted graph, assuming non-negative edge weights.

In this project:

Vertices: Rooms in the building.

Edges: Corridors or passages connecting rooms.

Edge Weights: Travel distances or travel costs between rooms.

Source Vertex: The room where evacuation begins.

Destination Vertex: A designated emergency exit.

The algorithm calculates a shortest route to the destination. The evacuation system must additionally consider room hazards when choosing which routes are safe to use.

🔑 Key Features

Real-time environmental monitoring, subject to the implemented sensor and update configuration.

Identification of potentially hazardous rooms.

Graph-based building representation.

Shortest-path calculation using Dijkstra's algorithm.

Evacuation-route planning.

Integration of hardware sensing and software-based pathfinding.

🌍 Applications

Educational institutions

Office buildings

Hospitals

Shopping malls

Industrial facilities

Other buildings requiring emergency evacuation planning

🚀 Future Enhancements

Add more sensors for improved environmental monitoring.

Develop a real-time graphical evacuation map.

Add wireless communication between sensors and the central controller.

Integrate visual or audio evacuation alerts.

Improve route planning by dynamically updating edge weights according to hazard severity.

Add multiple-exit optimization and alternative-route suggestions.

👨‍💻 Project Team

Team Members:

CB.SC.U4CSE25120

CB.SC.U4CSE25125

CB.SC.U4CSE25135

CB.SC.U4CSE25147

📂 Repository Contents

This repository contains the source code, project documentation, and other implementation files associated with the Smart Evacuation Manager case study.

The exact contents depend on the files uploaded to this repository.

📖 Conclusion

The Smart Evacuation Manager combines environmental sensing with graph algorithms to support emergency evacuation planning. By monitoring room conditions and calculating evacuation routes, the proposed system aims to help occupants identify more suitable paths toward emergency exits.

Project: Smart Evacuation Manager
Domain: IoT, Embedded Systems, Graph Theory, and Emergency Management
