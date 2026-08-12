using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Text;

namespace Discord
{
    public partial class StoreManager
    {
        public IEnumerable<Entitlement> GetEntitlements()
        {
            var count = CountEntitlements();
            var entitlements = new List<Entitlement>();
            for (var i = 0; i < count; i++)
            {
                entitlements.Add(GetEntitlementAt(i));
            }
            return entitlements;
        }

        public IEnumerable<Sku> GetSkus()
        {
            var count = CountSkus();
            var skus = new List<Sku>();
            for (var i = 0; i < count; i++)
            {
                skus.Add(GetSkuAt(i));
            }
            return skus;
        }
    }
}
