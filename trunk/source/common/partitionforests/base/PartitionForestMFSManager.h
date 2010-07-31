/***
 * millipede: PartitionForestMFSManager.h
 * Copyright Stuart Golodetz, 2010. All rights reserved.
 ***/

#ifndef H_MILLIPEDE_PARTITIONFORESTMFSMANAGER
#define H_MILLIPEDE_PARTITIONFORESTMFSMANAGER

#include "PartitionForestMultiFeatureSelection.h"

namespace mp {

template <typename MFS>
class PartitionForestMFSManager
{
	//#################### TYPEDEFS ####################
private:
	typedef boost::shared_ptr<MFS> MFS_Ptr;
	typedef boost::shared_ptr<const MFS> MFS_CPtr;
public:
	typedef std::map<std::string,MFS_Ptr> MFSMap;

	//#################### COMMANDS ####################
private:
	struct SetActiveMFSCommand : Command
	{
		PartitionForestMFSManager *m_base;
		std::string m_name;
		boost::optional<std::pair<std::string,MFS_Ptr> > m_oldActiveMultiFeatureSelection;

		SetActiveMFSCommand(PartitionForestMFSManager *base, const std::string& name)
		:	Command("Set Active Feature Selection"), m_base(base), m_name(name)
		{}

		void execute()
		{
			typename MFSMap::iterator it = m_base->m_multiFeatureSelections.find(m_name);
			if(it != m_base->m_multiFeatureSelections.end())
			{
				m_oldActiveMultiFeatureSelection = m_base->m_activeMultiFeatureSelection;
				m_base->m_activeMultiFeatureSelection = std::make_pair(it->first, it->second);
				m_base->m_listeners.active_multi_feature_selection_changed();
			}
			else throw Exception("Multi-feature selection " + m_name + " does not exist");
		}

		void undo()
		{
			m_base->m_activeMultiFeatureSelection = *m_oldActiveMultiFeatureSelection;
			m_oldActiveMultiFeatureSelection.reset();
			m_base->m_listeners.active_multi_feature_selection_changed();
		}
	};

	//#################### LISTENERS ####################
public:
	struct Listener
	{
		virtual ~Listener() {}
		virtual void active_multi_feature_selection_changed() = 0;
	};

private:
	struct CompositeListener : CompositeListenerBase<Listener>
	{
		void active_multi_feature_selection_changed()
		{
			multicast(boost::bind(&Listener::active_multi_feature_selection_changed, _1));
		}
	};

	//#################### PRIVATE VARIABLES ####################
private:
	std::pair<std::string,MFS_Ptr> m_activeMultiFeatureSelection;
	ICommandManager_Ptr m_commandManager;
	CompositeListener m_listeners;
	MFSMap m_multiFeatureSelections;

	//#################### CONSTRUCTORS ####################
public:
	PartitionForestMFSManager(const std::string& initialMFSName, const MFS_Ptr& initialMFS)
	:	m_activeMultiFeatureSelection(initialMFSName, initialMFS), m_commandManager(new BasicCommandManager)
	{
		add_multi_feature_selection(initialMFSName, initialMFS);
	}

	//#################### PUBLIC METHODS ####################
public:
	const MFS_Ptr& active_multi_feature_selection()
	{
		return m_activeMultiFeatureSelection.second;
	}

	MFS_CPtr active_multi_feature_selection() const
	{
		return m_activeMultiFeatureSelection.second;
	}

	void add_multi_feature_selection(const std::string& mfsName, const MFS_Ptr& mfs)
	{
		m_multiFeatureSelections.insert(std::make_pair(mfsName, mfs));
	}

	void add_shared_listener(const boost::shared_ptr<Listener>& listener)
	{
		m_listeners.add_shared_listener(listener);
	}

	const MFSMap& multi_feature_selections() const
	{
		return m_multiFeatureSelections;
	}

	void set_active_multi_feature_selection(const std::string& name)
	{
		m_commandManager->execute(Command_Ptr(new SetActiveMFSCommand(this, name)));
	}

	void set_command_manager(const ICommandManager_Ptr& commandManager)
	{
		m_commandManager = commandManager;
		for(typename MFSMap::iterator it=m_multiFeatureSelections.begin(), iend=m_multiFeatureSelections.end(); it!=iend; ++it)
		{
			it->second->set_command_manager(commandManager);
		}
	}
};

}

#endif