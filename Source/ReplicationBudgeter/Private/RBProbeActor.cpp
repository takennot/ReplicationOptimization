#include "RBProbeActor.h"
#include "RBGameState.h"
#include "RBPolicy.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "RBPlayerController.h"

ARBProbeActor::ARBProbeActor()
{
    PrimaryActorTick.bCanEverTick = true;
    SetReplicates(true);
    SetReplicateMovement(true);
    SetNetUpdateFrequency(RBPolicy::BaselineHz);
    SetMinNetUpdateFrequency(2.0f);
    SetNetCullDistanceSquared(FMath::Square(100000.0f));

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cube"));
    SetRootComponent(Mesh);
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetRelativeScale3D(FVector(0.37f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded())
    {
        Mesh->SetStaticMesh(Cube.Object);
    }
}

void ARBProbeActor::Configure(int32 NewId, const FVector& NewHome)
{
    if (!HasAuthority()) return;
    ProbeId = NewId;
    Home = NewHome;
    SetActorLocation(Home);
}

void ARBProbeActor::SetUpdateHz(float NewHz)
{
    if (!HasAuthority()) return;
    // Only change when crossing a policy band; avoid waking the net driver every tick.
    if (!FMath::IsNearlyEqual(GetNetUpdateFrequency(), NewHz))
    {
        SetNetUpdateFrequency(NewHz);
        SetMinNetUpdateFrequency(FMath::Min(2.0f, NewHz));
        ForceNetUpdate();
    }
}

void ARBProbeActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (HasAuthority())
    {
        Elapsed += DeltaSeconds;
        const float Phase = Elapsed * 1.65f + static_cast<float>(ProbeId) * 0.41f;
        // Deliberately continuously varying movement, so transform replication
        // is observable. No local client prediction or interpolation is performed.
        const FVector Next = Home + FVector(0.0f, FMath::Sin(Phase) * 110.f,
                                            FMath::Cos(Phase * 0.7f) * 80.f);
        SetActorLocation(Next);
    }

    if (GetNetMode() == NM_DedicatedServer || ProbeId < 0) return;
    const ARBPlayerController* PC = Cast<ARBPlayerController>(GetWorld()->GetFirstPlayerController());
    if (!PC || !PC->IsDebugEnabled() || !PC->GetPawn()) return;

    const float D = FVector::Dist(GetActorLocation(), PC->GetPawn()->GetActorLocation());
    const ARBGameState* State = GetWorld()->GetGameState<ARBGameState>();
    const bool bPrioritized = State && State->bOptimized;
    const FColor Color = !bPrioritized ? FColor(180, 190, 200)
                       : (IsCritical() ? FColor(225, 85, 235)
                       : (D < RBPolicy::NearCm ? FColor(35, 215, 220)
                       : (D < RBPolicy::MiddleCm ? FColor(240, 190, 70)
                       : FColor(115, 130, 148))));
    DrawDebugBox(GetWorld(), GetActorLocation(), FVector(24.0f), Color,
                 false, 0.0f, 0, 1.5f);
}

void ARBProbeActor::OnRep_ReplicatedMovement()
{
    Super::OnRep_ReplicatedMovement();
    ++MovementReceiveCount;
    LastMovementReceiveSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
}

float ARBProbeActor::GetNetPriority(const FVector& ViewPos, const FVector& ViewDir,
    AActor* Viewer, AActor* ViewTarget, UActorChannel* InChannel,
    float Time, bool bLowBandwidth)
{
    const float EnginePriority = Super::GetNetPriority(ViewPos, ViewDir, Viewer,
                                            ViewTarget, InChannel, Time, bLowBandwidth);
    const ARBGameState* State = GetWorld() ? GetWorld()->GetGameState<ARBGameState>() : nullptr;
    if (!State || !State->bOptimized) return EnginePriority;

    const float Distance = FVector::Dist(GetActorLocation(), ViewPos);
    return EnginePriority * RBPolicy::PriorityMultiplier(Distance, IsCritical(), Time);
}

void ARBProbeActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ARBProbeActor, ProbeId);
}
