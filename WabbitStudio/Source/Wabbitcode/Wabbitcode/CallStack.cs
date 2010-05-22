using System.Collections.Generic;
using System.Windows.Forms;
using Revsoft.Wabbitcode.Classes;
using Revsoft.Wabbitcode.Services;

namespace Revsoft.Wabbitcode.Docking_Windows
{
    public partial class CallStack : ToolWindow
    {
        public CallStack()
        {
            InitializeComponent();
        }

        public void addStackData(int address, int data)
        {
            string dataString = data.ToString("X4");
            /*if (data > 0x8000)
            {
                List<string> possibles = GlobalClass.mainForm.symTable.findEntryByValue(data.ToString("X4"));
                if (possibles.Count > 0)
                    foreach (string value in possibles)
                        dataString += " (" + value.ToLower() +")";
            }*/
            DataGridViewRow row = new DataGridViewRow();
            callStackView.Rows.Insert(0, row);
            callStackView.Rows[0].Cells[0].Value = address.ToString("X4");
            callStackView.Rows[0].Cells[1].Value = dataString;
        }

        public void removeLastRow()
        {
            if (callStackView.Rows.Count == 0)
                return;
            callStackView.Rows.Remove(callStackView.Rows[0]);
        }

        private void callStackView_DoubleClick(object sender, System.EventArgs e)
        {
			if (callStackView.SelectedRows.Count != 0)
				DebuggerService.GotoAddress(ushort.Parse(callStackView.Rows
										[callStackView.SelectedRows[0].Index].Cells[1].Value.ToString(),
											System.Globalization.NumberStyles.HexNumber));
        }

        public void Copy()
        {
            Clipboard.SetDataObject(callStackView.GetClipboardContent());
        }

		internal void Clear()
		{
			callStackView.Rows.Clear();
		}
	}
}