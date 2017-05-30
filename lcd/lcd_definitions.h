/*
 * lcd_definitions.h
 *
 * Created: 4/24/2017 8:34:36 AM
 *  Author: Bernd
 */ 

 // This file will only be used if the symbol _LCD_DEFINITIONS_FILE is defined (i.e. -D_LCD_DEFINITIONS_FILE)!

#ifndef LCD_DEFINITIONS_H_
#define LCD_DEFINITIONS_H_

#define LCD_PORT         PORTC        /**< port for the LCD lines   */
#define LCD_DATA0_PORT   LCD_PORT     /**< port for 4bit data bit 0 */
#define LCD_DATA1_PORT   LCD_PORT     /**< port for 4bit data bit 1 */
#define LCD_DATA2_PORT   LCD_PORT     /**< port for 4bit data bit 2 */
#define LCD_DATA3_PORT   LCD_PORT     /**< port for 4bit data bit 3 */
#define LCD_DATA0_PIN    0            /**< pin for 4bit data bit 0  */
#define LCD_DATA1_PIN    1            /**< pin for 4bit data bit 1  */
#define LCD_DATA2_PIN    2            /**< pin for 4bit data bit 2  */
#define LCD_DATA3_PIN    3            /**< pin for 4bit data bit 3  */
#define LCD_RS_PORT      PORTD        /**< port for RS line         */
#define LCD_RS_PIN       5            /**< pin 11 for RS line         */
#define LCD_RW_PORT      PORTD        /**< port for RW line         */
#define LCD_RW_PIN       6            /**< pin 12 for RW line         */
#define LCD_E_PORT       PORTD        /**< port for Enable line     */
#define LCD_E_PIN        7            /**< pin 13 for Enable line     */

#endif /* LCD_DEFINITIONS_H_ */