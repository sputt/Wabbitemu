﻿Imports System.Windows.Media.Animation
Imports System.Linq
Imports System.ComponentModel
Imports System.Collections.ObjectModel

Public Class MainWindow

    Public Shared ReadOnly ScenarioProperty As DependencyProperty =
        DependencyProperty.Register("Scenario", GetType(Scenario), GetType(MainWindow),
                                    New PropertyMetadata(New Scenario))

    Public Property Scenario As Scenario
        Get
            Return GetValue(ScenarioProperty)
        End Get
        Set(value As Scenario)
            SetValue(ScenarioProperty, value)
        End Set
    End Property

    'Public Shared Scenario As New Scenario
    Public Shared Instance As MainWindow

    Public Sub New()
        Instance = Me
        InitializeComponent()
    End Sub

    Private Sub Window_Loaded(sender As System.Object, e As System.Windows.RoutedEventArgs) Handles MyBase.Loaded
        Scenario.Tilesets.Add("dungeon", New Tileset("Scenario\dungeon.bmp"))
        TileSelectorPanel.Initialize(Scenario.Tilesets("dungeon"))

        Scenario.LoadScenario("Scenario\pete.asm")


        'Dim MapData As New MapData(New Tileset("dungeon.bmp"))
        'MapData.ZObjects = New ObservableCollection(Of ZObject)

        'Dim ZObj As New ZObject("Pot", 12, 12)
        'ZObj.X = 40
        'ZObj.Y = 20
        'MapData.ZObjects.Add(ZObj)

        'Dim ZObj2 As New ZObject("Pot", 20, 20)
        'MapData.ZObjects.Add(ZObj2)

        'ObjLayer.DataContext = MapData
        'ObjLayer.SetBinding(ObjectLayer.ItemsSourceProperty, New Binding("ZObjects"))
    End Sub

    Private Sub Background_MouseWheel(sender As System.Object, e As System.Windows.Input.MouseWheelEventArgs) Handles Background.MouseWheel
        Dim DefaultZoomFactor = 1.4
        Dim zoomFactor = DefaultZoomFactor

        Dim physicalPoint As Point = e.GetPosition(Background)

        Dim tt As TranslateTransform = CType(LayerContainer.RenderTransform, TransformGroup).Children.First(Function(t) TypeOf t Is TranslateTransform)
        Dim st As ScaleTransform = CType(LayerContainer.RenderTransform, TransformGroup).Children.First(Function(t) TypeOf t Is ScaleTransform)

        Dim mousePosition = LayerContainer.RenderTransform.Inverse.Transform(physicalPoint)

        If e.Delta <= 0 Then zoomFactor = 1.0 / DefaultZoomFactor
        Dim currentZoom = st.ScaleX

        Dim conv = New ValueConverters.DoubleToZoomLevelConverter
        CurrentZoomLevelItem.Content = conv.Convert(currentZoom, GetType(String), Nothing, Nothing)
        ZoomLevelCombo.SelectedIndex = 0

        currentZoom *= zoomFactor

        currentZoom = Math.Min(4.0, Math.Max(0.5, currentZoom))

        tt.BeginAnimation(TranslateTransform.XProperty, CreateZoomAnimation(-1 * (mousePosition.X * currentZoom - physicalPoint.X)), HandoffBehavior.Compose)
        tt.BeginAnimation(TranslateTransform.YProperty, CreateZoomAnimation(-1 * (mousePosition.Y * currentZoom - physicalPoint.Y)), HandoffBehavior.Compose)

        st.BeginAnimation(ScaleTransform.ScaleXProperty, CreateZoomAnimation(currentZoom), HandoffBehavior.Compose)
        st.BeginAnimation(ScaleTransform.ScaleYProperty, CreateZoomAnimation(currentZoom), HandoffBehavior.Compose)

        If currentZoom < 2.0 Then
            RenderOptions.SetBitmapScalingMode(LayerContainer, BitmapScalingMode.Fant)
        Else
            RenderOptions.SetBitmapScalingMode(LayerContainer, BitmapScalingMode.NearestNeighbor)
        End If
    End Sub

    Private StartPoint As Point
    Private Origin As Point

    Private Sub Background_MouseDown(sender As System.Object, e As System.Windows.Input.MouseButtonEventArgs) Handles Background.MouseRightButtonDown
        Dim tt As TranslateTransform = CType(LayerContainer.RenderTransform, TransformGroup).Children.First(Function(t) TypeOf t Is TranslateTransform)

        StartPoint = e.GetPosition(Background)
        Origin = New Point(tt.X, tt.Y)
        Background.CaptureMouse()
    End Sub

    Private Sub Background_MouseUp(sender As System.Object, e As System.Windows.Input.MouseButtonEventArgs) Handles Background.MouseRightButtonUp
        Background.ReleaseMouseCapture()
    End Sub

    Private Sub Background_Click(sender As System.Object, e As System.Windows.Input.MouseButtonEventArgs) Handles Background.MouseLeftButtonUp
        Scenario.ActiveLayer.DeselectAll()
    End Sub

    Private Sub ZoomAnimationEnd(sender As Object, e As EventArgs)
        Dim clock As Clock = sender

        If clock.CurrentState <> ClockState.Active Then
            Dim st As ScaleTransform = CType(LayerContainer.RenderTransform, TransformGroup).Children.First(Function(t) TypeOf t Is ScaleTransform)
            Dim conv = New ValueConverters.DoubleToZoomLevelConverter
            CurrentZoomLevelItem.Content = conv.Convert(st.ScaleX, GetType(String), Nothing, Nothing)
        End If
    End Sub

    Private Function CreateZoomAnimation(toValue As Double) As DoubleAnimation
        Dim da = New DoubleAnimation(toValue, New Duration(TimeSpan.FromMilliseconds(250)))
        da.AccelerationRatio = 0.1
        da.DecelerationRatio = 0.9
        AddHandler da.CurrentStateInvalidated, AddressOf ZoomAnimationEnd
        da.Freeze()

        Return da
    End Function

    Private Sub Background_MouseMove(sender As System.Object, e As System.Windows.Input.MouseEventArgs) Handles Background.MouseMove
        If Background.IsMouseCaptured Then
            Dim tt As TranslateTransform = CType(LayerContainer.RenderTransform, TransformGroup).Children.First(Function(t) TypeOf t Is TranslateTransform)

            Dim physicalPoint = e.GetPosition(Background)

            Dim toX = physicalPoint.X - StartPoint.X + Origin.X
            Dim toY = physicalPoint.Y - StartPoint.Y + Origin.Y

            If tt.HasAnimatedProperties Then
                tt.ApplyAnimationClock(TranslateTransform.XProperty, Nothing)
                tt.ApplyAnimationClock(TranslateTransform.YProperty, Nothing)
            End If

            tt.X = toX
            tt.Y = toY
        End If
    End Sub

    Private Sub ZoomLevelCombo_SelectionChanged(sender As System.Object, e As System.Windows.Controls.SelectionChangedEventArgs) Handles ZoomLevelCombo.SelectionChanged
        Dim combo As ComboBox = sender

        Dim newzoom As Double
        Dim sel As String = combo.SelectedItem.Content
        Try
            Dim NumStr As String = sel.Replace("%", "")
            newzoom = Double.Parse(NumStr) / 100.0
        Catch ex As Exception
            Debug.WriteLine(ex)
        End Try

        Try
            If Not LayerContainer Is Nothing AndAlso Not LayerContainer.RenderTransform Is Nothing Then
                Dim tt As TranslateTransform = CType(LayerContainer.RenderTransform, TransformGroup).Children.First(Function(t) TypeOf t Is TranslateTransform)
                Dim st As ScaleTransform = CType(LayerContainer.RenderTransform, TransformGroup).Children.First(Function(t) TypeOf t Is ScaleTransform)

                Dim PhysPos As New Point(Background.ActualWidth / 2, Background.ActualHeight / 2)
                Dim MousePos = LayerContainer.RenderTransform.Inverse.Transform(PhysPos)
                tt.BeginAnimation(TranslateTransform.XProperty, CreateZoomAnimation(-1 * (MousePos.X * newzoom - PhysPos.X)), HandoffBehavior.Compose)
                tt.BeginAnimation(TranslateTransform.YProperty, CreateZoomAnimation(-1 * (MousePos.Y * newzoom - PhysPos.Y)), HandoffBehavior.Compose)

                st.BeginAnimation(ScaleTransform.ScaleXProperty, CreateZoomAnimation(newzoom), HandoffBehavior.Compose)
                st.BeginAnimation(ScaleTransform.ScaleYProperty, CreateZoomAnimation(newzoom), HandoffBehavior.Compose)
            End If
        Catch ex As Exception

        End Try
    End Sub

    Private Sub ZoomLevelCombo_DropDownClosed(sender As System.Object, e As System.EventArgs) Handles ZoomLevelCombo.DropDownClosed
        CurrentZoomLevelItem.ClearValue(ComboBoxItem.HeightProperty)
    End Sub

    Private Sub ZoomLevelCombo_DropDownOpened(sender As System.Object, e As System.EventArgs) Handles ZoomLevelCombo.DropDownOpened
        CurrentZoomLevelItem.Height = 0
    End Sub

    Private Sub LayerRadioButton_Checked(sender As System.Object, e As System.Windows.RoutedEventArgs) Handles RadioMapSet.Checked, RadioMapView.Checked, RadioObjectLayer.Checked
        If Not LayerContainer Is Nothing Then
            Dim ActivateLayer = Sub(t As System.Type, rb As RadioButton)
                                    Dim AllLayers = (From s As MapContainer In LayerContainer.Children
                                                      From m In s.Children
                                                      Where m.GetType() = t
                                                      Select m).ToList()
                                    AllLayers.ForEach(Sub(s) s.Active = rb.IsChecked)
                                End Sub

            ActivateLayer(GetType(MapSet), RadioMapSet)
            ActivateLayer(GetType(MapView), RadioMapView)
            ActivateLayer(GetType(ObjectLayer), RadioObjectLayer)

            If RadioMapSet.IsChecked Then
                Scenario.ActiveLayerType = GetType(MapSet)
            ElseIf RadioMapView.IsChecked Then
                Scenario.ActiveLayerType = GetType(MapView)
            ElseIf RadioObjectLayer.IsChecked Then
                Scenario.ActiveLayerType = GetType(ObjectLayer)
            End If
        End If
    End Sub

    Private Sub MapsetNew_Click(sender As System.Object, e As System.Windows.RoutedEventArgs) Handles MapsetNew.Click
        Dim x = Grid.GetColumn(MapSet.CurrentlySelected)
        Dim y = Grid.GetRow(MapSet.CurrentlySelected)

        Dim MapData As New MapData(Scenario.Tilesets("dungeon"))

        Scenario.AddMap(x, y, MapData)

    End Sub

    Private Sub MenuItem1_Click(sender As System.Object, e As System.Windows.RoutedEventArgs) Handles MenuItem1.Click
        Scenario.SaveScenario()
    End Sub

    Private Sub Button1_Click(sender As System.Object, e As System.Windows.RoutedEventArgs) Handles Button1.Click
        Scenario.SaveScenario()
    End Sub

    Private Sub AddColumnLeft_Click(sender As System.Object, e As System.Windows.RoutedEventArgs) Handles AddColumnLeft.Click
        LayerContainer.AddLeftColumn()
        For Each Child In LayerContainer.Children
            Grid.SetColumn(Child, Grid.GetColumn(Child) + 1)
        Next
        For i = 0 To LayerContainer.RowDefinitions.Count - 1
            Scenario.AddMap(0, i, Nothing)
        Next
    End Sub

    Private Sub DeleteMapColumn_Click(sender As System.Object, e As System.Windows.RoutedEventArgs) Handles DeleteMapColumn.Click
        Dim CurCol = Grid.GetColumn(MapSet.CurrentlySelected)

        Dim LayersInColumn = (From m In LayerContainer.Children Where Grid.GetColumn(m) = CurCol).ToList()
        LayersInColumn.ForEach(Sub(m) LayerContainer.Children.Remove(m))

        LayerContainer.ColumnDefinitions.RemoveAt(CurCol)
        For Each Child In (From m In LayerContainer.Children Where Grid.GetColumn(m) > CurCol)
            Grid.SetColumn(Child, Grid.GetColumn(Child) - 1)
        Next
    End Sub

    Private Sub AddColumnRight_Click(sender As System.Object, e As System.Windows.RoutedEventArgs) Handles AddColumnRight.Click
        LayerContainer.AddRightColumn()
        For i = 0 To LayerContainer.RowDefinitions.Count - 1
            Scenario.AddMap(LayerContainer.ColumnDefinitions.Count - 1, i, Nothing)
        Next
    End Sub

    Private Sub AddRowTop_Click(sender As System.Object, e As System.Windows.RoutedEventArgs) Handles AddRowTop.Click
        LayerContainer.AddTopRow()
        For Each Child In LayerContainer.Children
            Grid.SetRow(Child, Grid.GetRow(Child) + 1)
        Next
        For i = 0 To LayerContainer.ColumnDefinitions.Count - 1
            Scenario.AddMap(i, 0, Nothing)
        Next
    End Sub

    Private Sub AddRowBottom_Click(sender As System.Object, e As System.Windows.RoutedEventArgs) Handles AddRowBottom.Click
        LayerContainer.AddBottomRow()
        For i = 0 To LayerContainer.ColumnDefinitions.Count - 1
            Scenario.AddMap(i, LayerContainer.RowDefinitions.Count - 1, Nothing)
        Next
    End Sub

    Private Sub DeleteMapRow_Click(sender As System.Object, e As System.Windows.RoutedEventArgs) Handles DeleteMapRow.Click
        Dim CurRow = Grid.GetRow(MapSet.CurrentlySelected)

        Dim LayersInRow = (From m In LayerContainer.Children Where Grid.GetRow(m) = CurRow).ToList()
        LayersInRow.ForEach(Sub(m) LayerContainer.Children.Remove(m))

        LayerContainer.RowDefinitions.RemoveAt(CurRow)
        For Each Child In (From m In LayerContainer.Children Where Grid.GetRow(m) > CurRow)
            Grid.SetRow(Child, Grid.GetRow(Child) - 1)
        Next
    End Sub
End Class