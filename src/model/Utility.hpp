#pragma once

namespace mm {

class UtilityFunction {
public:
    virtual ~UtilityFunction() = default;
    virtual double evaluate(double wealth) const = 0;
};

class IdentityUtility final : public UtilityFunction {
public:
    double evaluate(double wealth) const override { return wealth; }
};

}  // namespace mm
