# Industrial-Sorter PIC LCD EEPROM

> This project utilizes the MikroC Pro for PIC LCD library in 4-bit mode to drive a 16x2 character display.  
The firmware is highly optimized for the PIC16F628A, utilizing 99% of available ROM. 

**Disclaimer:** The code is stable for hobbyist/educational purposes. NOT FOR USE IN CRITICAL APPLICATIONS. IMPLEMENT AT YOUR OWN RISK.  

An automated sorting system designed for conveyor belt management and object categorization.   
This project integrates a PIC16F628A microcontroller with a 16x2 LCD HMI to provide real-time process control, job counting, and power-loss recovery.  

---

🚀 System Overview   

This project automates conveyor belt operations with the following features:  
* **Power-Loss Recovery:** Uses EEPROM to store the last box count and system parameters, resuming job progress after a power cycle.  
* **Live Parameter Tuning:** Adjust sorting times on the fly without interrupting the current cycle; updating target quantities triggers a clean job reset.  
* **Safety-Critical Logic:** Continuous monitoring of an Emergency Stop and Escape (ESC) button, combined with an automated timeout system to detect jammed objects.  
* **Industrial Workflow:** Logic gates ensure operation only while the system is clear, the job is incomplete, and no safety interrupts are triggered.  

---

🛠 Hardware Stack 

* **Microcontroller:** PIC16F628A (@ 4MHz Internal Oscillator)  
* **HMI:** 16x2 LCD (4-bit mode)  
* **Communication:** GPIO / Parallel Interface for LCD  

---

📂 Project Structure 

```
/PIC_Source          — The MikroC_project.zip contains all IDE files with compiled .hex + a screenshot of Fuse Bits and a copy of the main firmware for easy review.  
/Photos_Screenshots  — Visual overview of all 9 possible HMI states/pages + project photo.  
```

---

⚙ Operational Parameters   
* **Sorting Time:** from 0m 0s to 1m 59s. Default: 3s
* **Objects per Box:** from 1 to 999. Default: 5 units
* **Job Size (Boxes):** from 1 to 99. Default: 4 boxes
* **Object Timeout:** parameter fixed: ~30s
*Note: The `ONE_SECOND` definition must be empirically fine-tuned for accurate timeout tracking.*

---

## 📜 License

This project is licensed under the **Murat-Tech Source Available License v1.0**.  
Free for personal and educational use with attribution.  
Commercial use requires written authorization — contact info@murat-tech.eu  

---

☕ If this project is helpful for your application, please consider supporting:<br> 
https://www.paypal.com/donate/?hosted_button_id=8S8BJ9TT368VN  

Built by **rafamuratt**: https://murat-tech.eu/  
Murat-Tech Channel: https://www.youtube.com/@Murat-TechChannel-EN
