using System;
using System.Windows.Input;

namespace Gui
{
    // WPF에는 ICommand 기본 구현체가 없어서 직접 만듦 - CanExecute는
    // CommandManager.RequerySuggested(포커스 이동/클릭 등)에 맞춰 재평가됨.
    public class RelayCommand : ICommand
    {
        private readonly Action<object> execute_;
        private readonly Func<object, bool> canExecute_;

        public RelayCommand(Action<object> execute, Func<object, bool> canExecute = null)
        {
            execute_ = execute ?? throw new ArgumentNullException(nameof(execute));
            canExecute_ = canExecute;
        }

        public bool CanExecute(object parameter) => canExecute_?.Invoke(parameter) ?? true;

        public void Execute(object parameter) => execute_(parameter);

        public event EventHandler CanExecuteChanged
        {
            add { CommandManager.RequerySuggested += value; }
            remove { CommandManager.RequerySuggested -= value; }
        }
    }
}
