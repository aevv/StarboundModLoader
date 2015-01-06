using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace TestModule
{
    public class Class1
    {
        private static string api = "http://pokeapi.co/api/v1/ability/4/";

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void TestCallback(int admin);

        public static TestCallback test;

        static int Entry(string msg)
        {
            try
            {
                char[] chars = msg.ToCharArray();
                int ptr = BitConverter.ToInt32(Array.ConvertAll(chars, c => (byte)c), 0);
                test = (TestCallback)Marshal.GetDelegateForFunctionPointer(new IntPtr(ptr), typeof(TestCallback));
                Task.Factory.StartNew(ShowSettings);
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message);
            }
            return 0;
        }

        private static void ShowSettings()
        {
            new Settings().ShowDialog();
        }

        private static void TimePoll()
        {
            DateTime start = DateTime.Now;
            while (true)
            {
                if (DateTime.Now.Subtract(start).Seconds > 5)
                {
                    start = DateTime.Now;
                    WebRequest req = HttpWebRequest.Create(api);
                    req.Proxy = null;
                    var resp = req.GetResponse();
                    MessageBox.Show(new StreamReader(resp.GetResponseStream()).ReadToEnd());
                }
            }
        }

        private static void GetTime()
        {
            WebRequest req = HttpWebRequest.Create(api);
            req.Proxy = null;
            var resp = req.GetResponse();
            MessageBox.Show(new StreamReader(resp.GetResponseStream()).ReadToEnd());
        }
    }
}
