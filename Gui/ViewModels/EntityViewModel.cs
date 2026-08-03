using System;

namespace Gui.ViewModels
{
    // 서버 좌표(월드) -> 캔버스 픽셀로 매핑하는 heading-up(내 기수가 항상 화면 위쪽) 뷰.
    // 카메라 중심(캔버스 중앙)은 매 폴링마다 "내 엔티티"의 현재 위치/헤딩으로 갱신됨
    // (MainViewModel.Poll -> UpdateCanvasPosition) - 카메라가 나를 따라가면서 내 헤딩만큼 같이 돈다.
    // 카메라 기준 상대 좌표가 시야(WorldExtent)를 벗어나면 캔버스 가장자리에 clamp.
    public class EntityViewModel : ViewModelBase
    {
        public const double CanvasSize = 640.0;
        public const double WorldExtent = 300.0; // 카메라 중심 기준 -300..+300 이 캔버스에 다 들어감
        private const double MarkerRadius = 8.0;

        // 미니맵 - 회전도 카메라 추적도 없는, 월드 원점(0,0) 고정 절대 좌표 뷰
        public const double MinimapSize = 150.0;
        public const double MinimapWorldExtent = 400.0; // 원점 기준 -400..+400 이 미니맵에 다 들어감
        private const double MinimapMarkerRadius = 3.0;

        private uint entityId_;
        private float positionX_;
        private float positionY_;
        private float heading_;
        private bool isMine_;
        private double canvasLeft_;
        private double canvasTop_;
        private double headingDegrees_;
        private double minimapLeft_;
        private double minimapTop_;

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
            SetProperty(ref heading_, heading, nameof(Heading));
        }

        // cameraWorldX/Y = 캔버스 중앙에 놓일 월드 좌표, cameraHeading = 내 헤딩(라디안).
        // 내 헤딩이 항상 "화면 위"가 되도록, 카메라 기준 상대 위치/헤딩을 (90도 - cameraHeading)만큼
        // 같이 회전시킨다 - 위치와 헤딩 표시가 같은 회전을 공유해야 화면이 하나의 강체처럼 일관되게 돈다.
        public void UpdateCanvasPosition(double cameraWorldX, double cameraWorldY, double cameraHeading)
        {
            double relativeX = positionX_ - cameraWorldX;
            double relativeY = positionY_ - cameraWorldY;

            double delta = Math.PI / 2.0 - cameraHeading;
            double cosDelta = Math.Cos(delta);
            double sinDelta = Math.Sin(delta);
            double rotatedX = relativeX * cosDelta - relativeY * sinDelta;
            double rotatedY = relativeX * sinDelta + relativeY * cosDelta;

            SetProperty(ref canvasLeft_, ToCanvas(rotatedX) - MarkerRadius, nameof(CanvasLeft));
            SetProperty(ref canvasTop_, ToCanvas(-rotatedY) - MarkerRadius, nameof(CanvasTop)); // 화면 Y축은 아래로 증가하므로 반전

            // WPF의 회전 각도는 시계 방향(+)이고 물리 모델의 Heading은 반시계 방향 라디안이라 부호를 뒤집음.
            // 내 엔티티는 heading_ == cameraHeading이라 매번 -90도로 고정되어 항상 위를 가리킨다.
            double effectiveHeadingAngle = heading_ + delta;
            SetProperty(ref headingDegrees_, -effectiveHeadingAngle * 180.0 / Math.PI, nameof(HeadingDegrees));

            // 미니맵은 카메라 오프셋/회전을 전혀 안 받음 - 항상 월드 원점 기준 절대 좌표.
            // 축은 일부러 (X,Y)를 그대로 안 쓰고 (-Y, -X)로 씀 - heading=0(정지 상태 기본 헤딩)이
            // 월드 +X 방향인데, 그걸 "위"로 매핑해야 메인 레이더(heading-up)의 "위"와
            // 시작 시점 기준이 맞음. 그대로 (X,-Y)를 썼으면 미니맵의 "위"가 월드 +Y가 되어,
            // 처음 원점에 있을 때부터 두 화면의 "위"가 90도 어긋나 보였음.
            SetProperty(ref minimapLeft_, ToMinimap(-positionY_) - MinimapMarkerRadius, nameof(MinimapLeft));
            SetProperty(ref minimapTop_, ToMinimap(-positionX_) - MinimapMarkerRadius, nameof(MinimapTop));
        }

        public float PositionX => positionX_;
        public float PositionY => positionY_;
        public float Heading => heading_;
        public double HeadingDegrees => headingDegrees_;

        public double CanvasLeft => canvasLeft_;
        public double CanvasTop => canvasTop_;

        public double MinimapLeft => minimapLeft_;
        public double MinimapTop => minimapTop_;

        private static double ToCanvas(double relativeWorldCoordinate)
        {
            double normalized = (relativeWorldCoordinate + WorldExtent) / (WorldExtent * 2.0) * CanvasSize;
            return Math.Max(0.0, Math.Min(CanvasSize, normalized));
        }

        private static double ToMinimap(double worldCoordinate)
        {
            double normalized = (worldCoordinate + MinimapWorldExtent) / (MinimapWorldExtent * 2.0) * MinimapSize;
            return Math.Max(0.0, Math.Min(MinimapSize, normalized));
        }
    }
}
