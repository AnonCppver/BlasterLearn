#include "ProjectileBullet.h"
#include "Kismet/GameplayStatics.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Blaster/BlasterComponent/LagCompensationComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

AProjectileBullet::AProjectileBullet()
{
	ProjectileMovementComponent->InitialSpeed = InitialSpeed;
	ProjectileMovementComponent->MaxSpeed = InitialSpeed;
}

void AProjectileBullet::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	ABlasterCharacter* OwnerCharacter = Cast<ABlasterCharacter>(GetOwner());
	ABlasterCharacter* HitCharacter = Cast<ABlasterCharacter>(OtherActor);
	ABlasterPlayerController* OwnerController = OwnerCharacter ? Cast<ABlasterPlayerController>(OwnerCharacter->Controller) : nullptr;

	//const bool bWillDirectDamage = OwnerCharacter && OwnerController && HasAuthority() && (!bUseServerSideRewind || OwnerCharacter->IsLocallyControlled());
	//const bool bWillSendSSR = OwnerCharacter && OwnerController && bUseServerSideRewind && OwnerCharacter->GetLagCompensation() && OwnerCharacter->IsLocallyControlled() && HitCharacter;

	if (OwnerCharacter && OwnerController)
	{
		if (OwnerCharacter->HasAuthority() && OwnerCharacter->IsLocallyControlled())
		{
			const float DamageToCause = Hit.BoneName.ToString() == FString("Head") ? HeadShotDamage : Damage;
			UGameplayStatics::ApplyDamage(OtherActor, DamageToCause, OwnerController, this, UDamageType::StaticClass());
			Super::OnHit(HitComp, OtherActor, OtherComp, NormalImpulse, Hit);
			return;
		}
		if (bUseServerSideRewind && OwnerCharacter->GetLagCompensation() && OwnerCharacter->IsLocallyControlled() && HitCharacter)
		{
			const float HitTime = OwnerController->GetServerTime() - OwnerController->SingleTripTime;
			OwnerCharacter->GetLagCompensation()->ProjectileServerScoreRequest(
				HitCharacter,
				TraceStart,
				InitialVelocity,
				HitTime
			);
		}
	}

	Super::OnHit(HitComp, OtherActor, OtherComp, NormalImpulse, Hit);
}

void AProjectileBullet::BeginPlay()
{
	Super::BeginPlay();
}
