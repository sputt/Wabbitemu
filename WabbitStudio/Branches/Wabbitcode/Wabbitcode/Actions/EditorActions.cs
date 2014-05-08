﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows.Forms;
using Revsoft.TextEditor;
using Revsoft.TextEditor.Actions;
using Revsoft.Wabbitcode.GUI.Dialogs;
using Revsoft.Wabbitcode.GUI.DockingWindows;
using Revsoft.Wabbitcode.Interfaces;
using Revsoft.Wabbitcode.Properties;
using Revsoft.Wabbitcode.Services;
using Revsoft.Wabbitcode.Services.Interfaces;
using Revsoft.Wabbitcode.Services.Parser;
using Revsoft.Wabbitcode.Utils;

namespace Revsoft.Wabbitcode.Actions
{
    public class GotoDefinitionAction : AbstractUiAction
    {
        private readonly FilePath _fileName;
        private readonly string _text;
        private readonly int _currentLine;
        private readonly IDockingService _dockingService;
        private readonly IParserService _parserService;
        private readonly IProjectService _projectService;
        private readonly IFileService _fileService;
        private readonly FindResultsWindow _findResults;

        public GotoDefinitionAction(FilePath fileName, string text, int currentLine)
        {
            _fileName = fileName;
            _text = text;
            _currentLine = currentLine;
            _dockingService = ServiceFactory.Instance.GetServiceInstance<IDockingService>();
            _fileService = ServiceFactory.Instance.GetServiceInstance<IFileService>();
            _parserService = ServiceFactory.Instance.GetServiceInstance<IParserService>();
            _projectService = ServiceFactory.Instance.GetServiceInstance<IProjectService>();
            _findResults = _dockingService.GetDockingWindow(FindResultsWindow.WindowIdentifier) as FindResultsWindow;
        }

        public override void Execute()
        {
            IList<IParserData> parserData;
            if (_text.StartsWith("+") || _text.StartsWith("-") || _text == "_")
            {
                int steps = _text.Count(c => c == '+') - _text.Count(c => c == '-');
                if (steps > 0)
                {
                    steps--;
                }
                var parserInfo = _parserService.GetParserInfo(_fileName);
                List<ILabel> reusableLabels = parserInfo.LabelsList.Where(l => l.IsReusable).ToList();
                ILabel currentLabel = reusableLabels.FirstOrDefault(l => l.Location.Line >= _currentLine);
                if (currentLabel == null)
                {
                    return;
                }

                int index = reusableLabels.IndexOf(currentLabel) + steps;
                parserData = new List<IParserData> { reusableLabels[index] };
            }
            else
            {
                parserData = _parserService.GetParserData(_text, Settings.Default.CaseSensitive).ToList();
            }

            if (parserData.Count == 1)
            {
                new GotoLabelAction(parserData.Single()).Execute();
            }
            else
            {
                _findResults.NewFindResults(_text, _projectService.Project.ProjectName);
                foreach (IParserData data in parserData)
                {
                    string line = _fileService.GetLine(data.Parent.SourceFile, data.Location.Line + 1);
                    _findResults.AddFindResult(data.Parent.SourceFile, data.Location.Line, line);
                }
                _findResults.DoneSearching();
                _dockingService.ShowDockPanel(_findResults);
            }
        }

        public static void FromDialog()
        {
            GotoSymbol gotoSymbolBox = new GotoSymbol();
            gotoSymbolBox.ShowDialog();
        }
    }

    public class GotoFileAction : AbstractUiAction
    {
        private readonly FilePath _fileName;

        public GotoFileAction(FilePath fileName)
        {
            _fileName = fileName;
        }

        public override void Execute()
        {
            new OpenFileAction(_fileName).Execute();
        }
    }

    public class GotoLabelAction : AbstractUiAction
    {
        private readonly IParserData _parserData;

        public GotoLabelAction(IParserData parserData)
        {
            _parserData = parserData;
        }

        public override void Execute()
        {
            ParserInformation info = _parserData.Parent;
            FilePath file = info.SourceFile;
            new GotoLineAction(file, _parserData.Location.Line).Execute();
        }
    }

    public class GotoLineAction : AbstractUiAction
    {
        private readonly IDockingService _dockingService;
        private readonly DocumentLocation _location;

        public GotoLineAction()
        {
            _dockingService = ServiceFactory.Instance.GetServiceInstance<IDockingService>();
        }

        public GotoLineAction(DocumentLocation location) : this()
        {
            _location = location;
        }

        public GotoLineAction(FilePath fileName, int lineNumber)
            : this(new DocumentLocation(fileName, lineNumber))
        {
        }

        public override void Execute()
        {
            var editor = _dockingService.ActiveDocument as ITextEditor;
            int line;
            if (_location == null)
            {
                if (editor == null)
                {
                    return;
                }

                line = ShowGotoLineForm(editor);
            }
            else
            {
                new GotoFileAction(_location.FileName).Execute();
                line = _location.LineNumber;
                editor = _dockingService.Documents.OfType<ITextEditor>()
                    .Single(d => d.FileName == _location.FileName);
            }

            if (editor == null || line == -1)
            {
                return;
            }

            editor.GotoLine(line);
        }

        private static int ShowGotoLineForm(ITextEditor editor)
        {
            GotoLine gotoBox = new GotoLine(editor.TotalLines);
            DialogResult gotoResult = gotoBox.ShowDialog();
            if (gotoResult != DialogResult.OK)
            {
                return -1;
            }

            return Convert.ToInt32(gotoBox.inputBox.Text);
        }
    }

    public class ShowCodeCompletion : IEditAction
    {
        public Keys[] Keys { get; set; }
        public void Execute(TextArea textArea)
        {
            // TODO
        }
    }
}
