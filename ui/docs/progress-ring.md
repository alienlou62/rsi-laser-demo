# ProgressRing Usage Examples

**✨ Fixed: Smooth rotation performance during arc animation!**

## Basic Usage

### Simple Spinning Ring

```xml
<controls:ProgressRing
    Width="50"
    Height="50"
    IsSpinning="True"
    RingColor="Red"
    RingThickness="4" />
```

### Static Ring (No Animation)

```xml
<controls:ProgressRing
    Width="60"
    Height="60"
    IsSpinning="False"
    ArcLength="75"
    RingColor="Blue"
    RingThickness="3" />
```

## Arc Length Animation Feature

### Basic Arc Animation

The ring spins **and** the arc length animates between min/max values:

```xml
<controls:ProgressRing
    Width="80"
    Height="80"
    IsSpinning="True"
    IsArcAnimationEnabled="True"
    MinArcLength="20"
    MaxArcLength="80"
    ArcAnimationDuration="3"
    RingColor="Green"
    RingThickness="5" />
```

### Fast Arc Animation

Quick breathing effect with fast arc animation:

```xml
<controls:ProgressRing
    Width="40"
    Height="40"
    IsSpinning="True"
    IsArcAnimationEnabled="True"
    MinArcLength="10"
    MaxArcLength="90"
    ArcAnimationDuration="1"
    RingColor="Orange"
    RingThickness="3" />
```

### Arc Animation Only (No Spinning)

Arc length changes but ring doesn't rotate:

```xml
<controls:ProgressRing
    Width="70"
    Height="70"
    IsSpinning="False"
    IsArcAnimationEnabled="True"
    MinArcLength="30"
    MaxArcLength="70"
    ArcAnimationDuration="2.5"
    RingColor="Purple"
    RingThickness="4" />
```

## All Available Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `RingColor` | IBrush | Red | Color of the ring |
| `RingThickness` | double | 4.0 | Thickness of the ring stroke |
| `IsSpinning` | bool | true | Whether the ring rotates |
| `AnimationDuration` | double | 2.0 | Duration of spin rotation (seconds) |
| `ArcLength` | double | 75.0 | Static arc length (0-100% of circle) |
| `IsArcAnimationEnabled` | bool | false | Enable arc length animation |
| `MinArcLength` | double | 20.0 | Minimum arc length for animation (0-100%) |
| `MaxArcLength` | double | 80.0 | Maximum arc length for animation (0-100%) |
| `ArcAnimationDuration` | double | 3.0 | Duration of arc animation (seconds) |

## Animation Behavior

- **Spin Animation**: Continuous clockwise rotation when `IsSpinning="True"`
- **Arc Animation**: Alternates between MinArcLength and MaxArcLength when `IsArcAnimationEnabled="True"`
- **Combined**: Both animations can run simultaneously for a dynamic effect
- **Arc Animation Pattern**: Goes from min → max → min → max (alternating/ping-pong)

## Design Preview

The control includes design-time preview examples showing:
1. Red ring with arc animation (30-90%, 2s duration)
2. Blue static ring (no animation)
3. Green small ring with fast arc animation (20-80%, 1.5s duration)  
4. Orange ring with slow arc animation (10-70%, 4s duration)
