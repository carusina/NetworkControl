using System;
using System.Globalization;
using System.Windows.Data;

namespace Gui.Views
{
    // 연결된 뒤에는 Host/Port 입력을 잠그기 위해 IsConnected를 반전시켜 IsEnabled에 바인딩
    public class InverseBooleanConverter : IValueConverter
    {
        public static readonly InverseBooleanConverter Instance = new InverseBooleanConverter();

        public object Convert(object value, Type targetType, object parameter, CultureInfo culture) => !(bool)value;

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture) =>
            throw new NotSupportedException();
    }
}
