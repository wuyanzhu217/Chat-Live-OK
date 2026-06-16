#include "EffectChainFactory.h"

#include "plugins/BlurEffect.h"
#include "plugins/EqEffect.h"
#include "plugins/FlipEffect.h"
#include "plugins/GrayscaleEffect.h"
#include "plugins/SharpenEffect.h"

std::shared_ptr<EffectChain> createDefaultEffectChain()
{
    auto chain = std::make_shared<EffectChain>();
    chain->addEffect(std::make_shared<EqEffect>());
    chain->addEffect(std::make_shared<SharpenEffect>());
    chain->addEffect(std::make_shared<BlurEffect>());
    chain->addEffect(std::make_shared<GrayscaleEffect>());
    chain->addEffect(std::make_shared<FlipEffect>());
    return chain;
}
