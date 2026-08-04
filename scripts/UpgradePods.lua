a2fo.require_api(1, 2)

-- Bounded native extension for ResearchPod upgradeLevel values. Armada's
-- internal arrays remain safely projected onto tier 3; A2FO retains the real
-- tier and applies the highest attached pod for each system.
a2fo.configure_upgrade_pods({ maximum_tier = 6 })
a2fo.log("Upgrade-pod tiers enabled through tier 6")
