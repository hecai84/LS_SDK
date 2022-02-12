import os
import subprocess
file_path = os.path.basename(__file__)
root_path = os.path.realpath(os.path.join(file_path,'../'))
os.chdir(root_path)

def le501x_keil_proj_gen(path):
    os.chdir(path)
    process = subprocess.Popen(['scons','tool=progen'],shell=True)
    process.wait()
    os.chdir(root_path)

le501x_keil_proj_gen('examples/ble/ble_ancs')
le501x_keil_proj_gen('examples/ble/ble_at_proj')
le501x_keil_proj_gen('examples/ble/ble_dis')
le501x_keil_proj_gen('examples/ble/ble_dis_freertos')
le501x_keil_proj_gen('examples/ble/ble_fota_server')
le501x_keil_proj_gen('examples/ble/ble_hid')
le501x_keil_proj_gen('examples/ble/ble_hid_dmic')
le501x_keil_proj_gen('examples/ble/ble_mult_roles')
le501x_keil_proj_gen('examples/ble/ble_single_role')
le501x_keil_proj_gen('examples/ble/ble_uart_server')
le501x_keil_proj_gen('examples/ble/ble_uart_server_mult_link')
le501x_keil_proj_gen('examples/ble/fota')
le501x_keil_proj_gen('examples/ble/host_test')
le501x_keil_proj_gen('examples/peripheral/adc/adc_blocking_sampling')
le501x_keil_proj_gen('examples/peripheral/adc/adc_multi_channel')
le501x_keil_proj_gen('examples/peripheral/adc/adc_multi_channel_dma')
le501x_keil_proj_gen('examples/peripheral/adc/adc_single_channel')
le501x_keil_proj_gen('examples/peripheral/adc/adc_analog_wdg')
le501x_keil_proj_gen('examples/peripheral/crypt/ecb_cbc_it')
le501x_keil_proj_gen('examples/peripheral/crypt/ecb_cbc_polling')
le501x_keil_proj_gen('examples/peripheral/gpio')
le501x_keil_proj_gen('examples/peripheral/i2c/i2c_it')
le501x_keil_proj_gen('examples/peripheral/i2c/i2c_mem_polling')
le501x_keil_proj_gen('examples/peripheral/i2c/i2c_polling')
le501x_keil_proj_gen('examples/peripheral/pdm')
le501x_keil_proj_gen('examples/peripheral/rtc')
le501x_keil_proj_gen('examples/peripheral/software_calendar')
le501x_keil_proj_gen('examples/peripheral/software_timers')
le501x_keil_proj_gen('examples/peripheral/spi/spi_dma')
le501x_keil_proj_gen('examples/peripheral/spi/spi_it')
le501x_keil_proj_gen('examples/peripheral/spi/spi_polling')
le501x_keil_proj_gen('examples/peripheral/ssi/ssi_dma')
le501x_keil_proj_gen('examples/peripheral/ssi/ssi_it')
le501x_keil_proj_gen('examples/peripheral/timer/Basic_PWM')
le501x_keil_proj_gen('examples/peripheral/timer/Basic_TIM')
le501x_keil_proj_gen('examples/peripheral/timer/DTC_PWM')
le501x_keil_proj_gen('examples/peripheral/timer/Input_Capture')
le501x_keil_proj_gen('examples/peripheral/uart/uart_dma')
le501x_keil_proj_gen('examples/peripheral/uart/uart_it')
le501x_keil_proj_gen('examples/peripheral/uart/uart_polling')
le501x_keil_proj_gen('examples/prop_24g/ls_prop_24g')
le501x_keil_proj_gen('examples/mesh/genie_mesh/tmall_mesh')
le501x_keil_proj_gen('examples/mesh/genie_mesh/tmall_mesh_gatt')
le501x_keil_proj_gen('examples/mesh/genie_mesh/tmall_mesh_gatt_ls_ota')
le501x_keil_proj_gen('examples/mesh/genie_mesh/tmall_mesh_gatt_glp')
le501x_keil_proj_gen('examples/mesh/ls_mesh')
le501x_keil_proj_gen('examples/mesh/sig_mesh/sig_mesh_provee')
le501x_keil_proj_gen('examples/mesh/sig_mesh/sig_mesh_provee_alexa')
le501x_keil_proj_gen('examples/mesh/sig_mesh/sig_mesh_provee_auto_prov')
le501x_keil_proj_gen('examples/mesh/sig_mesh/sig_mesh_provisioner')
