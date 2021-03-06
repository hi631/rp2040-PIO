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
			this.MouseWheel += new MouseEventHandler(Form1_MouseWheel);
        }

		private void com_ctrl() {
			try{
				if (serialPort1.IsOpen==false){
					serialPort1.BaudRate = 115200;
					serialPort1.Parity = Parity.None;
					serialPort1.DataBits = 8;
					serialPort1.StopBits = StopBits.One;
					serialPort1.Handshake = Handshake.None;
					serialPort1.RtsEnable = true;		// 念の為
					serialPort1.DtrEnable = true;		// RP2040では必要
					serialPort1.PortName = comname.Text;
					serialPort1.Open();
					button1.BackColor=Color.LightGreen;
					button1.Text = "切断";
				} else {
					serialPort1.Close();
					button1.UseVisualStyleBackColor=true;
					button1.Text = "接続";
				}
			}
			catch (Exception ) { button1.BackColor=Color.Red; }
		}
		private void com_list() {
           string[] ports = SerialPort.GetPortNames();
            foreach (string port in ports) { portComboBox.Items.Add(port); }

		}

		int wheel_ct = 1;
		int wheel_ctc = 1;
		private void Form1_MouseWheel(object sender, MouseEventArgs e) {
			if (e.Delta > 0) { // マウスホイール前転
				if(wheel_ctc<12) wheel_ctc++;
			}
			if (e.Delta < 0) { // マウスホイール後転
				if(wheel_ctc>0) wheel_ctc--;
			}
			int[] wheel_div = {1,2,5,10,20,50,100,200,500,1000,2000,5000};
			wheel_ct = wheel_div[wheel_ctc];
			int wheel_xp = (e.X - pictureBox1.Left) * 100 / pictureBox1.Width;
			byte_to_bit_show(readData, wheel_xp, wheel_ct);
		}

		private void button1_Click(object sender, EventArgs e) { com_ctrl(); }

        private void Form1_Load(object sender, EventArgs e) {
			com_list();
			com_ctrl();
			checkBox1.Checked=false;
		}

        private void button2_Click(object sender, EventArgs e) {
            if (serialPort1.IsOpen)
            {
                serialPort1.Write(sndclk.Text + "\r\n");
            }
        }

		private void button3_Click(object sender, EventArgs e) {
			Application.Exit();
		}

		private void button4_Click(object sender, EventArgs e) {
			if (serialPort1.IsOpen==true){
				rcv_ptr = 0;
				serialPort1.Write("D\r\n"); // キャプチャーコマンド発行
			}
		}
		private void button5_Click(object sender, EventArgs e) {
			gtest(e);
		}

		private void portComboBox_SelectedIndexChanged(object sender, EventArgs e) {
			comname.Text = portComboBox.Text;
		}

		private void checkBox1_CheckedChanged(object sender, EventArgs e) {
			if (checkBox1.Checked) {
				textBox1.Visible = true; pictureBox1.Visible = false;
				byte_to_hex_show(readData); // 受信データの16進表示

			} else {
				textBox1.Visible = false; pictureBox1.Visible = true;
			}
		}

		private int bstr2int(int dp, int dl) {
			int ans = 0;
			for(int i=0; i<dl; i++) ans = ans*10 + readData[dp+i] - 0x30;
			return ans;
		}
		static int rcvdtmax = (26+32768+16*2);
		static int rcv_ptr = 0, rcv_max = 0;
		static byte[] readData = new byte[rcvdtmax];
		static int rdbeg, rplen, rdlen, rddiv;
		private void serialPort1_DataReceived(object sender, SerialDataReceivedEventArgs e) {
			//byte[] readData = new byte[serialPort1.BytesToRead];
			int rdl = serialPort1.BytesToRead;
			serialPort1.Read(readData, rcv_ptr, rdl); //シリアルポートから受信
			rcv_ptr = rcv_ptr + rdl;
			if (rcv_ptr > 26) {	// ヘッダー部は受信済
				int cp = 0; int cc = 0, cd = 0;
				while(cp<26 && !(cc=='C' && cd =='D')) { cc = cd; cd = readData[cp++]; }
				rplen = bstr2int(cp+1,2); rdlen = bstr2int(cp+4,5); rddiv = bstr2int(cp+10,5);
				rdbeg = cp + 18; // ex. CD(04:00256:0001)
				rcv_max= rdbeg + (rdlen/8+2)*rplen;
			} else { rcv_max= rcvdtmax; }
			if(rcv_ptr>=rcv_max) {
				Invoke(new AppendTextDelegate(packet_restortion), readData);//表示用のdelegateを呼び出す
				//rcv_ptr = 0;
			}
		}

		delegate void AppendTextDelegate(byte[] text);
 		private void packet_restortion(byte[] data)	{ disp_data(data);	}

		private void disp_data(byte[] data) {
			//byte_to_hex_show(data); // 受信データの16進表示
			pdlen = 0; pdofs = 0; wheel_ct = 1;
			byte_to_bit_show(data, 50, 0);
		}

		private void textBox2_TextChanged(object sender, EventArgs e) {
			rcv_ptr = 0;
			serialPort1.Write("T" + sndclk.Text + "\r\n");
		}

		int pdlen = 0;
		int pdofs = 0;
		private void byte_to_bit_show(byte[] data, int cpos, int mugx) {
			pictureBox1.Image = new Bitmap(pictureBox1.Width, pictureBox1.Height) ;
			Graphics grfx     = Graphics.FromImage(pictureBox1.Image) ; // Graphics オブジェクト
			// 描画に使うフォント、ペン、ブラシの定義
			Font       font  = new Font("ＭＳ Ｐ ゴシック", 30F) ;
			Pen        pen   = new Pen(Color.Black) ;
			SolidBrush brush = new SolidBrush(Color.Black) ;

			int dxm = pictureBox1.Width;	// 描画エリア(横)
			int dym = pictureBox1.Height;	// 　　　　　(縦)
			int dxs= 1 ;					// 横方向移動距離
			int dys = dym/(rplen+2);		// 縦方向の間隔
			int sdlen = rdlen/8+2;			// 各pinデータの長さ(末尾にCR+LF)
			//
			int pdclkp = pdofs + (pdlen*cpos/100);	// 画面上のクリック位置(0-100)
			pdlen = rdlen/wheel_ct;
			pdofs = pdclkp - (pdlen/2);
			if(pdofs<0) pdofs = 0;
			if((pdofs+pdlen)>=rdlen) pdofs = pdofs = rdlen - pdlen;
			//
			int pgspan = adjspan(pdlen/20);		// グリッドの間隔
			if(pgspan<1) pgspan = 1;
			int pgdiv  = pgspan*rddiv*1000/125;	// グリッド当たりの幅(ns)
			sndclk.Text = rddiv.ToString();
			scale.Text = pgdiv.ToString();
			int pgcnt=0, pgcnto=0;
			//
			for(int pc=0; pc<(rplen+1); pc++){
				int pdtop = rdbeg+sdlen*pc;	// 各pinデータの先頭(バイト位置)
				int dyp = dys*(pc+1);		// 描画時の縦位置
				int dyd = 0, dydo=0;
				for(int dxp=0; dxp<dxm; dxp++) {
					int dix = pdlen * dxp / dxm + pdofs;	// 受信データ上の位置(ドット単位)
					if(pc<rplen) {
						int dd = data[pdtop+dix/8];			// 　　　　　　　　　(バイト単位)
						int bm = 0x80 >> (dix % 8);			// バイト中のビット位置
						if ((dd & bm) != 0) dyd = dys/2; 
						else                dyd = 0; 
						grfx.DrawLine( pen, dxp, dyp-dyd, dxp+dxs, dyp-dyd);
						if(dydo!=dyd) 
							grfx.DrawLine( pen, dxp, dyp-dydo, dxp, dyp-dyd);
						dydo = dyd;
					} else {
						// グリッドの表示
						pgcnt = dix/pgspan;
						if(pgcnt!=pgcnto) {
							grfx.DrawLine( pen, dxp, dyp+20, dxp, dyp);
						}
						pgcnto = pgcnt;
					}
				}
			}
			grfx.DrawLine( pen, 0, dys*(rplen+1), dxm, dys*(rplen+1));

		}

		private int adjspan(int sbase) { // スケールを切りの良い値に補正
			int aspan = sbase;
			int asift = 1;
			while(aspan>9) { aspan = aspan / 10; asift = asift * 10; }
			if(aspan>5) aspan = 10;
			else if(aspan>2) aspan = 5;
			aspan = aspan * asift;
			return aspan;
		}

		private void byte_to_hex_show(byte[] data) {
			string str = ""; string hex = "";
 			for (int i = 0; i < rcv_ptr; i++) {
				hex = data[i].ToString("X");
				if (hex.Length % 2 == 1) { hex = "0" + hex; }
				str += hex + " ";
			}
			textBox1.Text = str + "\r\n" + textBox1.Text;
			this.Update();
		}

		private void gtest(EventArgs e) {
			// Graphics オブジェクトの取得
			pictureBox1.Image = new Bitmap(pictureBox1.Width, pictureBox1.Height) ;
			Graphics grfx     = Graphics.FromImage(pictureBox1.Image) ;
			// フォント、ペン、ブラシの定義
			Font       font  = new Font("ＭＳ Ｐ ゴシック", 30F) ;
			Pen        pen   = new Pen(Color.Magenta) ;
			SolidBrush brush = new SolidBrush(Color.Orange) ;
			// 描画例
			grfx.DrawString( "abc", font, brush, 10, 30); // 文字列を描画 MSG,,,X,Y
			grfx.DrawLine( pen, 100, 30, 200, 100); // 直線を描画 X,Y,XE,YE
			grfx.FillRectangle( brush, 200, 30, 100, 40); // 四角形の内部を塗潰し X,Y,W,H
		}

	}
}
// 受信データ例(CAPTURE_MEM 128) 
// 44 0D 0D 0A 3E 0A 
// 43 44 28 30 34 3A 30 30 32 35 36 3A 30 30 30 30 31 29 0D 0A   <- CD(04:00256:0001) 
// FF FF FF FF FF FF FF E0 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 1F FF FF FF FF FF FF FF FF 0D 0A 
// 00 00 00 00 00 00 FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FC 0D 0A 
// 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 0D 0A 
// 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 0D 0A
