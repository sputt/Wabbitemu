﻿using Revsoft.Wabbitcode.GUI.Dialogs;
using Revsoft.Wabbitcode.Interfaces;
using Revsoft.Wabbitcode.Services;
using Revsoft.Wabbitcode.Services.Interfaces;

namespace Revsoft.Wabbitcode.Actions
{
    public class RefactorRenameAction : AbstractUiAction
    {
        private readonly IDockingService _dockingService;
        private readonly IProjectService _projectService;

        public RefactorRenameAction()
        {
            _dockingService = ServiceFactory.Instance.GetServiceInstance<IDockingService>();
            _projectService = ServiceFactory.Instance.GetServiceInstance<IProjectService>();
        }

        public override void Execute()
        {
            ITextEditor editor = _dockingService.ActiveDocument as ITextEditor;
            if (editor == null)
            {
                return;
            }

            RefactorRenameForm renameForm = new RefactorRenameForm(editor, _projectService);
            renameForm.ShowDialog();
        }
    }

    public class RefactorExtractMethodAction : AbstractUiAction
    {
        private readonly IDockingService _dockingService;
        private readonly IProjectService _projectService;

        public RefactorExtractMethodAction()
        {
            _dockingService = ServiceFactory.Instance.GetServiceInstance<IDockingService>();
            _projectService = ServiceFactory.Instance.GetServiceInstance<IProjectService>();
        }

        public override void Execute()
        {
            ITextEditor editor = _dockingService.ActiveDocument as ITextEditor;
            if (editor == null)
            {
                return;
            }

            RefactorExtractMethodForm renameForm = new RefactorExtractMethodForm(editor, _projectService);
            renameForm.ShowDialog();
        }
    }
}