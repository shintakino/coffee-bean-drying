# Coffee Bean Drying Automation System - Context

## Project Overview
This project implements an automated environmental control system specifically designed for coffee bean drying. The system maintains optimal drying conditions by monitoring and regulating temperature and humidity levels through a closed-loop control system, ensuring consistent quality and preventing over-drying or mold development.

## System Architecture

### Hardware Components
- **ESP32 Microcontroller**: Central control unit
- **DHT22 Sensor**: Measures temperature and humidity
- **I2C LCD Display**: Provides local status monitoring and settings display
- **Relay Module**: Controls actuators (heating mat, fans, humidifier)

### Cloud Infrastructure
- **Firebase**: Provides real-time data storage and synchronization
  - **Firestore Database**: Stores historical data and settings
  - **Realtime Database**: Handles real-time status updates and commands

### Web Dashboard
- **Vercel-hosted Application**: Provides remote monitoring and control
  - **Trends Visualization**: Shows historical temperature and humidity data
  - **Current Status**: Displays current environmental conditions
  - **Settings Management**: Allows for adjustment of target conditions
  - **Manual Override**: Provides direct control over actuators

## Control Logic

### Temperature Control
The system maintains temperature within a configurable range optimal for coffee bean drying:
- If temperature falls below minimum threshold: Activate heating mat
- If temperature exceeds maximum threshold: Activate cooling fans

### Humidity Control
The system maintains humidity within a configurable range to ensure proper moisture extraction:
- If humidity falls below minimum threshold: Activate humidifier (to prevent too-rapid drying)
- If humidity exceeds maximum threshold: Activate fans for air exchange

### LCD Display Modes
The system cycles through four information display modes:
1. Current temperature and humidity readings
2. Temperature range settings (min/max)
3. Humidity range settings (min/max)
4. Relay status indicators

## Data Flow
1. DHT22 sensor collects temperature and humidity data
2. ESP32 processes data and makes control decisions
3. ESP32 controls relays based on environmental conditions
4. ESP32 displays current status on LCD
5. ESP32 sends data to Firebase database
6. Web dashboard retrieves data from Firebase
7. User can view historical trends and current conditions
8. User can adjust settings through web dashboard
9. ESP32 receives updated settings from Firebase

## Coffee Bean Drying Considerations
- Typical drying temperature range: 30-35°C (86-95°F)
- Initial humidity level: High (50-70%)
- Target final moisture content: 10-12%
- Gradual reduction in humidity prevents bean cracking
- Consistent airflow ensures even drying
- Drying process typically lasts 7-14 days depending on conditions

## Safety Features
- Error detection for sensor failures
- Automatic shutdown on sensor failure
- Configurable alerts for out-of-range conditions
- Manual override capability
- Thermal runaway protection

## Physical Installation Recommendations
- Place temperature sensor near bean drying bed
- Position fans for optimal air circulation across bean layer
- Ensure air intake filters to prevent contamination
- Mount control box in accessible location away from moisture
- Provide adequate air circulation space

## Maintenance Requirements
- Regular cleaning of sensors
- Periodic validation of temperature and humidity readings
- Cleaning of air circulation components
- System backup battery check (if installed)

## Future Expansion Options
- Bean moisture content direct measurement
- Multiple zone temperature monitoring
- Batch tracking and logging
- Mobile app notifications
- Integration with roasting process data
- Machine learning for optimal drying profiles