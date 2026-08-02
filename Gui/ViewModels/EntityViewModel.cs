using System;

namespace Gui.ViewModels
{
    // 서버 좌표(월드) -> 캔버스 픽셀로 매핑하는 고정 배율 뷰.
    // 카메라 중심은 캔버스 중앙에 고정되지만, 그 중심이 가리키는 월드 좌표는 매 폴링마다
    // "내 엔티티"의 현재 위치로 갱신됨(MainViewModel.Poll -> UpdateCanvasPosition) - 즉 카메라가 나를 따라감.
    // 카메라 기준 상대 좌표가 시야(WorldExtent)를 벗어나면 캔버스 가장자리에 clamp.
    public class EntityViewModel : ViewModelBase
    {
        public const double CanvasSize = 640.0;
        public const double WorldExtent = 300.0; // 카메라 중심 기준 -300..+300 이 캔버스에 다 들어감
        private const double MarkerRadius = 8.0;

        private uint entityId_;
        private float positionX_;
        private float positionY_;
        private float heading_;
        private bool isMine_;
        private double canvasLeft_;
        private double canvasTop_;

        public uint EntityId
        {
            get => entityId_;
            set => SetProperty(ref entityId_, value);
        }

        public bool IsMine
        {
            get => isMine_;
            set => SetProperty(ref isMine_, value);
        }

        public void UpdateState(float positionX, float positionY, float heading)
        {
            SetProperty(ref positionX_, positionX, nameof(PositionX));
            SetProperty(ref positionY_, positionY, nameof(PositionY));
            if (SetProperty(ref heading_, heading, nameof(Heading))) { RaisePropertyChanged(nameof(HeadingDegrees)); }
        }

        // cameraWorldX/Y = 캔버스 중앙에 놓일 월드 좌표(=내 엔티티의 현재 위치)
        public void UpdateCanvasPosition(double cameraWorldX, double cameraWorldY)
        {
            SetProperty(ref canvasLeft_, ToCanvas(positionX_ - cameraWorldX) - MarkerRadius, nameof(CanvasLeft));
            SetProperty(ref canvasTop_, ToCanvas(-(positionY_ - cameraWorldY)) - MarkerRadius, nameof(CanvasTop)); // 화면 Y축은 아래로 증가하므로 반전
        }

        public float PositionX => positionX_;
        public float PositionY => positionY_;
        public float Heading => heading_;

        // WPF의 회전 각도는 시계 방향(+)이고, 물리 모델의 Heading은 반시계 방향 라디안이라 부호를 뒤집음
        public double HeadingDegrees => -heading_ * 180.0 / Math.PI;

        public double CanvasLeft => canvasLeft_;
        public double CanvasTop => canvasTop_;

        private static double ToCanvas(double relativeWorldCoordinate)
        {
            double normalized = (relativeWorldCoordinate + WorldExtent) / (WorldExtent * 2.0) * CanvasSize;
            return Math.Max(0.0, Math.Min(CanvasSize, normalized));
        }
    }
}
