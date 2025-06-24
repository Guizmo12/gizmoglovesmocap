# Mocap gloves for SlimeVR

## Hardware

- FlexSensor 2.2" x5  
<img src="https://github.com/user-attachments/assets/d7b9dc47-64e4-46c6-9428-821c17ad21d6" alt="flex sensor" width="350"/>  
<img src="https://github.com/user-attachments/assets/73aec79d-447e-4f99-bd3b-dd7476fa87de" alt="flex sensor" width="350"/>  

- 10k resistance x5  

- Gloves of your choice. I chose to put 2 fingerless glove to make it prettier  
<img src="https://github.com/user-attachments/assets/a67b9726-dd63-4628-ad4e-4eb43ddd0c52" alt="gloves" width="350"/>  

- Case: [I used this case](https://www.thingiverse.com/thing:4200147) and modified it to have a hole for the wire and added a backplate to sandwich the FeatherV2 to the case
<img src="https://github.com/user-attachments/assets/6e04c2a0-9dad-40e9-8da4-b8877253c835" alt="case" width="350"/>  

- ESP32: [FeatherV2 from AdaFruit](https://www.adafruit.com/product/5400)  
<img src="https://github.com/user-attachments/assets/1f44f644-999b-4f10-ac1b-c2d30fe99bfd" alt="featherV2" width="350"/>  

### **How to build:**  
I have cut some slit of the glove to waive the flexsensor trough.
You can use a different way of course, but be sure the base is fixed and the tip can move foward and backward easily

<img src="https://github.com/user-attachments/assets/8ab241df-8c4b-4965-ade1-8f581a6688fd" alt="build1" width="400"/>  

<img src="https://github.com/user-attachments/assets/f1bb9be3-0689-4c46-ab3f-3d9bbeb75066" alt="build2" width="400"/>  
<img src="https://github.com/user-attachments/assets/90622e37-aa24-46cf-9e00-bd2ff9889105" alt="build4" width="400"/>

Wiring is easy just solder wires to all of the same side of the flex sensor and solder to GND pin. And the other site to the Feather pins  
<img src="https://github.com/user-attachments/assets/d1ab0d50-5145-4e97-9453-d61a661a1002" alt="build3" width="400"/>  

Put the resistances on one side of the Featherv2 and solder all of them together. Solder a wire on the 3v to the joint resistances  
<img src="https://github.com/user-attachments/assets/dfd16e30-fd1e-48c2-b028-d1a7e98d9100" alt="build4" width="400"/>  


And solder the wires to the other side in that order:  
A9: Thumb  
A7: Index  
A2: Middle  
A3: Ring  
A4: Little  

<img src="https://github.com/user-attachments/assets/61e62f3a-48e1-4e59-940a-301538ade69c" alt="build5" width="400"/>  

<img src="https://github.com/user-attachments/assets/4174ca0c-9379-43f4-adf1-b5780762082f" alt="build6" width="700"/> 

Here I'm using the USB of the FeatherV2 to power it from the wrist tracker. 
<img src="https://github.com/user-attachments/assets/ee50a69e-d20b-411e-b293-925b1f2b499b" alt="build6" width="700"/>

## Code

You'll need the [Arduino IDE](https://www.arduino.cc/en/software)

- ConnectToSlime is for use with the [SlimeVR Server](https://github.com/SlimeVR/SlimeVR-Server)
- FullGloveWIFI is for use with server_tester.py
- FullGloveNoWiFi is for use with a serial console
