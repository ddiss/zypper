#include <iostream>

#include "zypp/base/Logger.h"
#include "zypp/base/Algorithm.h"
#include "zypp/Patch.h"
#include "zypp/Pattern.h"
#include "zypp/Product.h"

#include "zypp/ResPoolProxy.h"

#include "Zypper.h"
#include "main.h"
#include "Table.h"


#include "search.h"

using namespace zypp;
using namespace std;

extern ZYpp::Ptr God;


string selectable_search_repo_str(const ui::Selectable & s)
{
  string repostr;

  // show available objects
  for_(it, s.availableBegin(), s.availableEnd())
  {      
    if (repostr.empty())
      repostr = (*it)->repository().info().name();
    else if (repostr != (*it)->repository().info().name())
    {
      repostr = _("(multiple)");
      return repostr;
    }
  }

  return repostr;
}

string string_patch_status(const PoolItem & pi)
{
  // make sure this will not happen
  if (pi.isUndetermined())
    return _("Unknown");

  if (pi.isRelevant())
  {
    if (pi.isSatisfied())
      return _("Installed"); //! \todo make this "Applied" instead?
    if (pi.isBroken())
      return _("Needed");
    // can this ever happen?
    return "";
  }

  return _("Not Applicable"); //! \todo make this "Not Needed" after 11.0
}


static string string_weak_status(const ResStatus & rs)
{
  if (rs.isRecommended())
    return _("Recommended");
  if (rs.isSuggested())
    return _("Suggested");
  return ""; 
}


void list_patches(Zypper & zypper)
{
  MIL
    << "Pool contains " << God->pool().size()
    << " items. Checking whether available patches are needed." << std::endl;

  Table tbl;

  FillPatchesTable callback(tbl);
  invokeOnEach(
    God->pool().byKindBegin(ResKind::patch),
    God->pool().byKindEnd(ResKind::patch),
    callback);

  tbl.sort (1);                 // Name

  if (tbl.empty())
    zypper.out().info(_("No needed patches found."));
  else
    // display the result, even if --quiet specified
    cout << tbl;
}


void list_patterns(Zypper & zypper)
{
  MIL << "Going to list patterns." << std::endl;

  Table tbl;
  TableHeader th;

  // translators: S for installed Status
  th << _("S");
  th << _("Name") << _("Version");
  if (!zypper.globalOpts().is_rug_compatible)
    th << _("Repository") << _("Dependency"); 
  tbl << th;

  ResPool::byKind_iterator
    it = God->pool().byKindBegin(ResKind::pattern),
    e  = God->pool().byKindEnd(ResKind::pattern);
  for (; it != e; ++it )
  {
    Pattern::constPtr pattern = asKind<Pattern>(it->resolvable());

    TableRow tr;
    tr << (it->isSatisfied() ? "i" : ""); 
    tr << pattern->name () << pattern->edition().asString();
    if (!zypper.globalOpts().is_rug_compatible)
    {
      tr << pattern->repoInfo().name();
      tr << string_weak_status(it->status ());
    }
    tbl << tr;
  }
  tbl.sort(1); // Name

  if (tbl.empty())
    zypper.out().info(_("No patterns found."));
  else
    // display the result, even if --quiet specified
    cout << tbl;
}

void list_packages(Zypper & zypper)
{
  MIL << "Going to list packages." << std::endl;

  Table tbl;
  TableHeader th;

  // translators: S for installed Status
  th << _("S");
  if (zypper.globalOpts().is_rug_compatible)
    // translators: Bundle is a term used in rug. See rug for how to translate it.
    th << _("Bundle");
  else
    th << _("Repository");
  th << _("Name") << _("Version") << _("Arch");
  tbl << th;
  
  bool installed_only = zypper.cOpts().count("installed-only");
  bool notinst_only = zypper.cOpts().count("uninstalled-only");

  ResPoolProxy::const_iterator
    it = God->pool().proxy().byKindBegin(ResKind::package),
    e  = God->pool().proxy().byKindEnd(ResKind::package);
  for (; it != e; ++it )
  {
    ui::Selectable::constPtr s = *it;

    // get the first installed object
    PoolItem installed;
    if (!s->installedEmpty())
      installed = s->installedObj();

    // show available objects
    for_(it, s->availableBegin(), s->availableEnd())
    {
      TableRow row;

      zypp::PoolItem pi = *it;
      if (installed)
      {
        if (notinst_only)
          continue;
        row << (equalNVRA(*installed.resolvable(), *pi.resolvable()) ? "i" : "v");
      }
      else
      {
        if (installed_only)
          continue;
        row << "";
      }
      row << (zypper.globalOpts().is_rug_compatible ? "" : pi->repository().info().name())
          << pi->name()
          << pi->edition().asString()
          << pi->arch().asString();

      tbl << row;
    }
  }
  if (zypper.cOpts().count("sort-by-repo") || zypper.cOpts().count("sort-by-catalog"))
    tbl.sort(1); // Repo
  else
    tbl.sort(2); // Name

  if (tbl.empty())
    zypper.out().info(_("No packages found."));
  else
    // display the result, even if --quiet specified
    cout << tbl;
}

void list_products(Zypper & zypper)
{
  MIL << "Going to list packages." << std::endl;

  Table tbl;
  TableHeader th;

  // translators: S for installed Status
  th << _("S");
  th << _("Name");
  th << _("Version");
  if (zypper.globalOpts().is_rug_compatible)
     // translators: product category (the rug term)
     th << _("Category");
  else
    // translators: product type (addon/base) (rug calls it Category)
    th << _("Type");
  tbl << th;

  bool installed_only = zypper.cOpts().count("installed-only");
  bool notinst_only = zypper.cOpts().count("uninstalled-only");

  ResPool::byKind_iterator
    it = God->pool().byKindBegin(ResKind::product),
    e  = God->pool().byKindEnd(ResKind::product);
  for (; it != e; ++it )
  {
    Product::constPtr product = asKind<Product>(it->resolvable());

    TableRow tr;
    if (it->status().isInstalled())
    {
      if (notinst_only)
        continue;
      tr << "i";
    }
    else
    {
      if (installed_only)
        continue;
      tr << "";
    }
    tr << product->name () << product->edition().asString();
    tr << product->type();
    tbl << tr;
  }
  tbl.sort(1); // Name

  if (tbl.empty())
    zypper.out().info(_("No products found."));
  else
    // display the result, even if --quiet specified
    cout << tbl;
}


void list_what_provides(Zypper & zypper, const string & str)
{
  Capability cap = safe_parse_cap (zypper, /*kind,*/ str);
  sat::WhatProvides q(cap);

  // is there a provider for the requested capability?
  if (q.empty())
  {
    zypper.out().info(str::form(_("No providers of '%s' found."), str.c_str()));
    return;
  }

  Table t;
  invokeOnEach(q.selectableBegin(), q.selectableEnd(), FillSearchTableSolvable(t) );
  cout << t;
}

// Local Variables:
// c-basic-offset: 2
// End: