using System.Windows;
using Gui.ViewModels;

namespace Gui.Views
{
    public partial class MainWindow : Window
    {
        private readonly MainViewModel viewModel_ = new MainViewModel();

        public MainWindow()
        {
            InitializeComponent();
            DataContext = viewModel_;
            Closing += (_, __) => viewModel_.Dispose();
        }
    }
}
