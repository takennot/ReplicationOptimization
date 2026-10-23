#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RBProbeActor.generated.h"

class UStaticMeshComponent;

/** One moving, server-owned network object. No Blueprints or custom assets. */
UCLASS()
class REPLICATIONBUDGETER_API ARBProbeActor : public AActor
{
    GENERATED_BODY()
public:
    ARBProbeActor();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    virtual void OnRep_ReplicatedMovement() override;
    virtual float GetNetPriority(const FVector& ViewPos, const FVector& ViewDir,
        AActor* Viewer, AActor* ViewTarget, UActorChannel* InChannel,
        float Time, bool bLowBandwidth) override;

    void Configure(int32 NewId, const FVector& NewHome);
    void SetUpdateHz(float NewHz);
    bool IsCritical() const { return (ProbeId % 10) == 0; }
    int32 GetReceiveCount() const { return MovementReceiveCount; }
    double GetLastReceiveSeconds() const { return LastMovementReceiveSeconds; }
    int32 GetId() const { return ProbeId; }

private:
    UPROPERTY(VisibleAnywhere, Category="Probe")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(Replicated)
    int32 ProbeId = -1;

    FVector Home = FVector::ZeroVector;
    float Elapsed = 0.0f;
    int32 MovementReceiveCount = 0;
    double LastMovementReceiveSeconds = -1.0;
};
