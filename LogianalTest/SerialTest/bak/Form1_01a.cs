using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO.Ports;

namespace SerialTest
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();
        }

		private void com_ctrl() {
			try{
				if (serialPort1.IsOpen==false){
					serialPort1.BaudRate = 115200;
					serialPort1.Parity = Parity.None;
					serialPort1.DataBits = 8;
					serialPort1.StopBits = StopBits.One;
					serialPort1.Handshake = Handshake.None;
					serialPort1.RtsEnable=true;		// 念の為
					serialPort1.DtrEnable = true;	// RP2040では必要
					serialPort1.PortName = comname.Text; //portComboBox.Text;
					serialPort1.Open();
					button1.BackColor=Color.LightGreen;
				} else {
					serialPort1.Close();
					button1.UseVisualStyleBackColor=true;
				}
			}
			catch (Exception ) { button1.BackColor=Color.Red; }
		}
		private void com_list()
		{
           string[] ports = SerialPort.GetPortNames();
            foreach (string port in ports)
            {
                portComboBox.Items.Add(port);
            }
            //if (portComboBox.Items.Count > 0) portComboBox.SelectedIndex = 0;

		}
        private void button1_Click(object sender, EventArgs e) { com_ctrl(); }

        private void Form1_Load(object sender, EventArgs e) {
			com_list();
			com_ctrl();
		}

        private void button2_Click(object sender, EventArgs e) {
            if (serialPort1.IsOpen)
            {
                serialPort1.Write(textBox2.Text + "\r\n");
            }
        }

       private void button3_Click(object sender, EventArgs e) {
			Application.Exit();
		}

		private void button4_Click(object sender, EventArgs e) {
			rcv_ptr = 0;
			serialPort1.Write("D\r\n");
		}
		private void portComboBox_SelectedIndexChanged(object sender, EventArgs e) {
			comname.Text = portComboBox.Text;
		}

		private int bstr2int(int dp, int dl) {
			int ans = 0;
			for(int i=0; i<dl; i++) ans = ans*10 + readData[dp+i] - 0x30;
			return ans;
		}
		static int rcv_ptr = 0, rcv_max = 0;
		static byte[] readData = new byte[4*(16+8)+32];
		private void serialPort1_DataReceived(object sender, SerialDataReceivedEventArgs e) {
			//byte[] readData = new byte[serialPort1.BytesToRead];
			int rdl = serialPort1.BytesToRead;
			serialPort1.Read(readData, rcv_ptr, rdl); //シリアルポートから受信
			rcv_ptr = rcv_ptr + rdl;
			if (rcv_ptr > 13) {	// ヘッダーを受信済
				int plen = bstr2int(4,2); int dlen = bstr2int(7,5);
				rcv_max= (dlen/8+8)*plen+16;
			} else { rcv_max= 32768; }
			if(rcv_ptr>=rcv_max)
				Invoke(new AppendTextDelegate(packet_restortion), readData);//表示用のdelegateを呼び出す
		}
 
		delegate void AppendTextDelegate(byte[] text);
 		private void packet_restortion(byte[] data)	{ byte_to_hex_show(data);	}
		private void byte_to_hex_show(byte[] data) {
			string str = ""; string hex = "";
 			for (int i = 0; i < rcv_ptr; i++) {
				hex = data[i].ToString("X");
				if (hex.Length % 2 == 1) { hex = "0" + hex; }
				str += hex + " ";
				hex = "";
			}
			textBox1.Text = str + "\r\n" + textBox1.Text;
		}

	}
}
// 受信データ C(04:00128)  P0x:
// 44 0D 43 28 30 34 3A 30 30 31 32 38 29 0D 0A 
// 50 30 30 3A 0D 0A 00 00 00 00 00 00 0F FF FF FF FF FF FF FF FE 00 0D 0A 
// 50 30 31 3A 0D 0A 00 00 00 00 FF FF FF FF 80 00 00 00 3F FF FF FF 0D 0A 
// 50 30 32 3A 0D 0A 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 0D 0A 
// 50 30 33 3A 0D 0A 00 00 00 00 FF FF FF FF 80 00 00 00 3F FF FF FF 0D 0A 
// 0D 0A 3E 0A 
