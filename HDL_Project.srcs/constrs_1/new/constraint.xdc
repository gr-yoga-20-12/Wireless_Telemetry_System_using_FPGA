set_property CONFIG_VOLTAGE 3.3 [current_design]
set_property CFGBVS VCCO [current_design]

# Buzzer - AXI GPIO output (note the _tri_o[0] suffix)
set_property -dict { PACKAGE_PIN L19  IOSTANDARD LVCMOS33 } [get_ports gpio_io_o_0]

# UART to ESP32
set_property -dict { PACKAGE_PIN T19  IOSTANDARD LVCMOS33 } [get_ports uart_rtl_txd]
set_property -dict { PACKAGE_PIN R19  IOSTANDARD LVCMOS33 } [get_ports uart_rtl_rxd]

# XADC analog pins
# IOSTANDARD LVCMOS33 is required here to match Bank 35 VCCO = 3.3V
# It does NOT affect analog input behavior - XADC configures itself automatically

#LDR Sensor
set_property -dict { PACKAGE_PIN M19  IOSTANDARD LVCMOS33 } [get_ports Vaux2_0_v_p]
set_property -dict { PACKAGE_PIN M20  IOSTANDARD LVCMOS33 } [get_ports Vaux2_0_v_n]

#Temperature Sensor
set_property -dict { PACKAGE_PIN M17  IOSTANDARD LVCMOS33 } [get_ports Vaux10_0_v_p]
set_property -dict { PACKAGE_PIN M18  IOSTANDARD LVCMOS33 } [get_ports Vaux10_0_v_n]